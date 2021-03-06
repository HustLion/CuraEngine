/** Copyright (C) 2013 David Braam - Released under terms of the AGPLv3 License */
#ifndef GCODEEXPORT_H
#define GCODEEXPORT_H

#include <stdio.h>
#include <deque> // for extrusionAmountAtPreviousRetractions
#include <sstream> // for stream.str()

#include "settings.h"
#include "utils/intpoint.h"
#include "timeEstimate.h"

namespace cura {

struct CoastingConfig
{
    bool coasting_enable; 
    double coasting_volume_move; 
    double coasting_speed_move; 
    double coasting_min_volume_move; 

    double coasting_volume_retract;
    double coasting_speed_retract;
    double coasting_min_volume_retract;
};
    
class RetractionConfig
{
public:
    double amount; //!< The amount retracted
    double speed; //!< The speed with which to retract
    double primeSpeed; //!< the speed with which to unretract
    double primeAmount; //!< the amount of material primed after unretracting
    int zHop; //!< the amount with which to lift the head during a retraction-travel
};

//The GCodePathConfig is the configuration for moves/extrusion actions. This defines at which width the line is printed and at which speed.
class GCodePathConfig
{
private:
    double speed; //!< movement speed
    int line_width; //!< width of the line extruded
    int filament_diameter; //!< diameter of the filament as it is on the roll
    double flow; //!< extrusion flow in %
    int layer_thickness; //!< layer height
    double extrusion_volume_per_mm; //!< mm^3 filament moved per mm line extruded
    double extrusion_per_mm; //!< mm filament moved per mm line extruded
public:
    const char* name;
    bool spiralize;
    RetractionConfig* retraction_config;
    
    GCodePathConfig() : speed(0.0), line_width(0), extrusion_volume_per_mm(0), extrusion_per_mm(0), name(nullptr), spiralize(false), retraction_config(nullptr) {}
    GCodePathConfig(RetractionConfig* retraction_config, const char* name) : speed(0.0), line_width(0), extrusion_volume_per_mm(0), extrusion_per_mm(0), name(name), spiralize(false), retraction_config(retraction_config) {}
    
    void setSpeed(double speed)
    {
        this->speed = speed;
    }
    
    void setLineWidth(int line_width)
    {
        this->line_width = line_width;
        calculateExtrusion();
    }
    
    void setLayerHeight(int layer_height)
    {
        this->layer_thickness = layer_height;
        calculateExtrusion();
    }

    void setFilamentDiameter(int diameter)
    {
        this->filament_diameter = diameter;
        calculateExtrusion();
    }

    void setFlow(double flow)
    {
        this->flow = flow;
        calculateExtrusion();
    }
    
    void smoothSpeed(double min_speed, int layer_nr, double max_speed_layer) 
    {
        speed = (speed*layer_nr)/max_speed_layer + (min_speed*(max_speed_layer-layer_nr)/max_speed_layer);
    }
    
    //volumatric extrusion means the E values in the final GCode are cubic mm. Else they are in mm filament.
    double getExtrusionPerMM(bool volumatric)
    {
        if (volumatric)
            return extrusion_volume_per_mm;
        return extrusion_per_mm;
    }
    
    double getSpeed()
    {
        return speed;
    }
    
    int getLineWidth()
    {
        return line_width;
    }

private:
    void calculateExtrusion()
    {
        extrusion_volume_per_mm = INT2MM(line_width) * INT2MM(layer_thickness) * flow / 100.0;
        double filament_area = M_PI * (INT2MM(filament_diameter) / 2.0) * (INT2MM(filament_diameter) / 2.0);
        extrusion_per_mm = extrusion_volume_per_mm / filament_area;
    }
};

//The GCodeExport class writes the actual GCode. This is the only class that knows how GCode looks and feels.
//  Any customizations on GCodes flavors are done in this class.
class GCodeExport
{
private:
    std::ostream* output_stream;
    double extrusion_amount;
    double extruderSwitchRetraction;
    int extruderSwitchRetractionSpeed;
    int extruderSwitchPrimeSpeed;
    double retraction_extrusion_window;
    double retraction_count_max;
    std::deque<double> extrusion_amount_at_previous_n_retractions;
    Point3 currentPosition;
    Point3 startPosition;
    Point extruderOffset[MAX_EXTRUDERS];
    char extruderCharacter[MAX_EXTRUDERS];
    int currentTemperature[MAX_EXTRUDERS];
    double currentSpeed;
    int zPos;
    bool isRetracted;
    bool isZHopped;
    double last_coasted_amount; //!< The coasted amount of filament to be primed on the first next extrusion. (same type as GCodeExport::extrusion_amount)
    double retractionPrimeSpeed;
    int extruderNr;
    int currentFanSpeed;
    EGCodeFlavor flavor;
    std::string preSwitchExtruderCode[MAX_EXTRUDERS];
    std::string postSwitchExtruderCode[MAX_EXTRUDERS];
    
    double totalFilament[MAX_EXTRUDERS];
    double totalPrintTime;
    TimeEstimateCalculator estimateCalculator;
public:
    
    GCodeExport();
    ~GCodeExport();
    
    void setOutputStream(std::ostream* stream);
    
    void setExtruderOffset(int id, Point p);
    Point getExtruderOffset(int id);
    void setSwitchExtruderCode(int id, std::string preSwitchExtruderCode, std::string postSwitchExtruderCode);
    
    void setFlavor(EGCodeFlavor flavor);
    EGCodeFlavor getFlavor();
        
    void setRetractionSettings(int extruderSwitchRetraction, double extruderSwitchRetractionSpeed, double extruderSwitchPrimeSpeed, int minimalExtrusionBeforeRetraction, int retraction_count_max);
    
    void setZ(int z);
    
    void setLastCoastedAmount(double last_coasted_amount) { this->last_coasted_amount = last_coasted_amount; }
    
    Point3 getPosition();
    
    Point getPositionXY();
    
    void resetStartPosition();

    Point getStartPositionXY();

    int getPositionZ();

    int getExtruderNr();
    
    double getTotalFilamentUsed(int e);

    double getTotalPrintTime();
    void updateTotalPrintTime();
    void resetTotalPrintTime();
    
    void writeComment(std::string comment);
    void writeTypeComment(const char* type);
    void writeLayerComment(int layer_nr);
    
    void writeLine(const char* line);
    
    void resetExtrusionValue();
    
    void writeDelay(double timeAmount);
    
    void writeMove(Point p, double speed, double extrusion_per_mm);
    
    void writeMove(Point3 p, double speed, double extrusion_per_mm);
private:
    void writeMove(int x, int y, int z, double speed, double extrusion_per_mm);
public:
    void writeRetraction(RetractionConfig* config, bool force=false);
    
    void switchExtruder(int newExtruder);
    
    void writeCode(const char* str);
    
    void writeFanCommand(double speed);
    
    void writeTemperatureCommand(int extruder, double temperature, bool wait = false);
    void writeBedTemperatureCommand(double temperature, bool wait = false);
    
    void preSetup(SettingsBase& settings)
    {
        for(unsigned int n=1; n<MAX_EXTRUDERS;n++)
        {
            std::ostringstream stream_x; stream_x << "machine_nozzle_offset_x_" << n;
            std::ostringstream stream_y; stream_y << "machine_nozzle_offset_y_" << n;
            setExtruderOffset(n, Point(settings.getSettingInMicrons(stream_x.str()), settings.getSettingInMicrons(stream_y.str())));
//             else 
//                 std::cerr << " !!!!!!!!!!! cannot find setting " << stream_x.str() << " or " << stream_y.str() << std::endl;
        }
        for(unsigned int n=0; n<MAX_EXTRUDERS;n++)
        {
            std::ostringstream stream;
            stream << "_" << n;
            setSwitchExtruderCode(n, settings.getSettingString("machine_pre_extruder_switch_code" + stream.str()), settings.getSettingString("machine_post_extruder_switch_code" + stream.str()));
        }

        setFlavor(settings.getSettingAsGCodeFlavor("machine_gcode_flavor"));
        setRetractionSettings(settings.getSettingInMicrons("machine_switch_extruder_retraction_amount"), settings.getSettingInMillimetersPerSecond("material_switch_extruder_retraction_speed"), settings.getSettingInMillimetersPerSecond("material_switch_extruder_prime_speed"), settings.getSettingInMicrons("retraction_extrusion_window"), settings.getSettingInMicrons("retraction_count_max"));
    }
    void finalize(int maxObjectHeight, double moveSpeed, const char* endCode);
    
};

}

#endif//GCODEEXPORT_H
