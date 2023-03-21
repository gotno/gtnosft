#include "../dep/oscpack/ip/IpEndpointName.h"
#include "../dep/oscpack/ip/UdpSocket.h"
#include "../dep/oscpack/osc/OscPacketListener.h"
#include "../dep/oscpack/osc/OscPrintReceivedElements.h"
#include "../dep/oscpack/osc/OscReceivedElements.h"

#include "plugin.hpp"

class OscDebugPacketListener : public PacketListener{
public:
	void ProcessPacket(const char *data, int size, const IpEndpointName& remoteEndpoint) override {
    (void) remoteEndpoint; // suppress unused parameter warning

    osc::ReceivedPacket RxPacket(data, size);
    DEBUG("OSCRX-- size: %d\tdata: %s", RxPacket.Size(), RxPacket.Contents());
	}
};
