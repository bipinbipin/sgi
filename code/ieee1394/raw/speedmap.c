/* 
 *  This application prints the speedmap as defined in IEEE 1394 standard.
 */

#include <stdio.h>
#include <raw1394.h>

main(argc,argv)
int argc;
char **argv;
{
    RAW1394port port;
    RAW1394config config;
    RAW1394speedMap* speedMap;
    RAW1394topologyMap* topoMap;
    int i, j;

    config = raw1394NewConfig();
    if (config == NULL) {
        fprintf(stderr,"SpeedMap: raw1394NewConfig() returned null config.\n");
        exit(-1);
    }

    port = raw1394OpenPort(config);
    if (port == NULL) {
        fprintf(stderr,"SpeedMap: raw1394OpenPort() returned null port.\n");
        exit(-1);
    }

    topoMap = raw1394GetTopologyMap(port);
    speedMap = raw1394GetSpeedMap(port);
    printf("length=%d\n",speedMap->length);
    printf("crc=0x%x\n", speedMap->crc);
    printf("generation_number=%d\n",speedMap->generationNumber);

    for (i = 0; i < topoMap->nodeCount; i++)
        for (j = 0; j < topoMap->nodeCount; j++)
            printf("    Speed between node %d and %d = %d \n",
                        i, j, speedMap->matrix[64*i + j]);

    if (raw1394ClosePort(port) < 0) {
        fprintf(stderr, "SpeedMap: raw1394ClosePort() failed");
        exit(-1);
    }

    exit(0);
}
