/*
 *  This application displays the topology map as defined in IEEE 1394 standard.
 */

#include <stdio.h>
#include <raw1394.h>

main(argc,argv)
int argc;
char **argv;
{
    RAW1394port port;
    RAW1394config config;
    RAW1394topologyMap* topoMap;
    int more_packets, i;

    config = raw1394NewConfig();
    if (config == NULL) {
        fprintf(stderr,"TopoMap: raw1394NewConfig() returned null config.\n");
        exit(-1);
    }

    port = raw1394OpenPort(config);
    if (port == NULL) {
        fprintf(stderr,"TopoMap: raw1394OpenPort() returned null port.\n");
        exit(-1);
    }

    topoMap = raw1394GetTopologyMap(port);
    printf("length=%d\n",topoMap->length);
    printf("crc=0x%x\n",topoMap->crc);
    printf("generation_number=%d\n",topoMap->generationNumber);
    printf("node_count=%d\n",topoMap->nodeCount);
    printf("self_id_count=%d\n",topoMap->selfIdCount);
    
    more_packets=0;
    for (i = 0; i < topoMap->selfIdCount; i++) {
        if (!more_packets) {
           printf(" SelfID %d:  phy_ID = %d, linkActive = %d, gap_cnt = %d, phySpeed = %d, phyDelay = %d, contender = %d\n",
		  i, topoMap->selfIdPacket[i].packetZero.phyID,
		  topoMap->selfIdPacket[i].packetZero.linkActive, 
		  topoMap->selfIdPacket[i].packetZero.gapCount,
		  topoMap->selfIdPacket[i].packetZero.phySpeed, 
		  topoMap->selfIdPacket[i].packetZero.phyDelay,
		  topoMap->selfIdPacket[i].packetZero.contender);

	   printf("            pwr: %d, p0: %d, p1: %d, p2: %d, initiatedReset: %d, morePackets: %d\n",
		  topoMap->selfIdPacket[i].packetZero.powerClass,
		  topoMap->selfIdPacket[i].packetZero.port0,
		  topoMap->selfIdPacket[i].packetZero.port1,
		  topoMap->selfIdPacket[i].packetZero.port2,
		  topoMap->selfIdPacket[i].packetZero.initiatedReset,
		  topoMap->selfIdPacket[i].packetZero.morePackets);
	   
	   if (topoMap->selfIdPacket[i].packetZero.morePackets == 0) more_packets=0;
	   else more_packets=1;
	}
	else {
	   printf(" SelfID %d:  phy_ID = %d, packetNumber: %d, portA: %d, portB: %d, portC: %d\n",
			i, topoMap->selfIdPacket[i].packetMore.phyID,
			topoMap->selfIdPacket[i].packetMore.packetNumber,
			topoMap->selfIdPacket[i].packetMore.portA,
			topoMap->selfIdPacket[i].packetMore.portB,
			topoMap->selfIdPacket[i].packetMore.portC
		 );

	   printf("            portD: %d, portE: %d, portF: %d, portG: %d, portH: %d, morePackets: %d\n",
			topoMap->selfIdPacket[i].packetMore.portD,
			topoMap->selfIdPacket[i].packetMore.portE,
			topoMap->selfIdPacket[i].packetMore.portF,
			topoMap->selfIdPacket[i].packetMore.portG,
			topoMap->selfIdPacket[i].packetMore.portH,
			topoMap->selfIdPacket[i].packetMore.morePackets
		 );

           if (topoMap->selfIdPacket[i].packetZero.morePackets == 0) more_packets=0;
           else more_packets=1;
	}
    }

    if (raw1394ClosePort(port) < 0) {
        fprintf(stderr, "TopoMap: raw1394ClosePort() failed");
        exit(-1);
    }

    exit(0);
}
