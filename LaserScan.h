
#include "rplidar.h" //RPLIDAR standard sdk, all-in-one header
#include "BMP.h"
#include <math.h>


#define PI 3.14159265

#ifndef _countof
#define _countof(_Array) (int)(sizeof(_Array) / sizeof(_Array[0]))
#endif

using namespace rp::standalone::rplidar;

class LaserScan
{

public:
    LaserScan(const char *com_port_arg, _u32 baudrate_arg);
    bool start();
    bool stop();
    bool connect();
    void scan();


private:
	bool checkRPLIDARHealth(RPlidarDriver *drv);
	float getAngle(const rplidar_response_measurement_node_hq_t& node);
    bool is_connected;
    const char *com_port;
    _u32 baudrate;
    RPlidarDriver *drv;

};

LaserScan::LaserScan(const char *com_port_arg, _u32 baudrate_arg)
{
    com_port = com_port_arg;
    baudrate = baudrate_arg;
    is_connected = false;
    drv = RPlidarDriver::CreateDriver(DRIVER_TYPE_SERIALPORT);

    if (!drv)
    {
        fprintf(stderr, "insufficent memory, exit\n");
        exit(-2);
    }

    //BMP bmp2(2000, 2000);
    //bmp2.fill_region(250,1000,10,10,255,0,0,255);
}


void LaserScan::scan()
{
    rplidar_response_measurement_node_hq_t nodes[8192];
    size_t   count = _countof(nodes);
	u_result op_result = drv->grabScanDataHq(nodes, count);

    if (IS_OK(op_result))
    {
        drv->ascendScanData(nodes, count);

        for (int pos = 0; pos < (int)count ; ++pos)
        {
            float distance = nodes[pos].dist_mm_q2 / 4.0f;
            float angle = getAngle(nodes[pos]);
            uint32_t x = 250 + roundf(sin (angle * PI / 180) * distance) / 3;
            uint32_t y = 1000 + roundf(cos (angle * PI / 180) * distance) / 3;
            uint32_t quality = 3 * round(nodes[pos].quality >> RPLIDAR_RESP_MEASUREMENT_QUALITY_SHIFT);
            //bmp2.fill_region(x, y, 4, 4, quality, 0, 0, 255);
            printf("%s theta: %03.2f Dist: %08.2f Q: %d X: %d Y: %d\n",
                   (nodes[pos].quality & RPLIDAR_RESP_MEASUREMENT_SYNCBIT) ? "S " : "  ",
                   angle,
                   distance,
                   quality,
                   x,
                   y);
        }
        //bmp2.write("t1_24_copy.bmp");
    }
}

bool LaserScan::stop()
{
    is_connected = false;
    if(drv != NULL)
    {
        drv->stop();
        drv->stopMotor();
        RPlidarDriver::DisposeDriver(drv);
        drv = NULL;
    }
    return true;
}

bool LaserScan::start()
{
	u_result op_result;
    rplidar_response_device_info_t devinfo;

    if(!drv)
        drv = RPlidarDriver::CreateDriver(DRIVER_TYPE_SERIALPORT);

    if (IS_OK(drv->connect(com_port, baudrate)))
    {
        op_result = drv->getDeviceInfo(devinfo);

        if (!IS_OK(op_result))
        {
            delete drv;
            drv = NULL;
            fprintf(stderr, "Error, cannot bind to the specified serial port %s.\n", com_port);
            return false;
        }
    }

    // print out the device serial number, firmware and hardware version number..
    printf("RPLIDAR S/N: ");
    for (int pos = 0; pos < 16 ; ++pos)
    {
        printf("%02X", devinfo.serialnum[pos]);
    }

    printf("\n"
           "Firmware Ver: %d.%02d\n"
           "Hardware Rev: %d\n"
           , devinfo.firmware_version >> 8
           , devinfo.firmware_version & 0xFF
           , (int)devinfo.hardware_version);

    // check health...
    if (!checkRPLIDARHealth(drv))
    {
        return false;
    }

    drv->startMotor();
    // start scan...
    drv->startScan(0, 1);

    is_connected = true;
    return true;
}


bool LaserScan::checkRPLIDARHealth(RPlidarDriver *drv)
{
    u_result     op_result;
    rplidar_response_device_health_t healthinfo;

    op_result = drv->getHealth(healthinfo);
    if (IS_OK(op_result))   // the macro IS_OK is the preperred way to judge whether the operation is succeed.
    {
        printf("RPLidar health status : %d\n", healthinfo.status);
        if (healthinfo.status == RPLIDAR_STATUS_ERROR)
        {
            fprintf(stderr, "Error, rplidar internal error detected. Please reboot the device to retry.\n");
            // enable the following code if you want rplidar to be reboot by software
            // drv->reset();
            return false;
        }
        else
        {
            return true;
        }

    }
    else
    {
        fprintf(stderr, "Error, cannot retrieve the lidar health code: %x\n", op_result);
        return false;
    }
}


float LaserScan::getAngle(const rplidar_response_measurement_node_hq_t& node)
{
    return node.angle_z_q14 * 90.f / 16384.f;
}

