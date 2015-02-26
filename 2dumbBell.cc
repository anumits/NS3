#include <ctype.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cassert>

#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/point-to-point-dumbbell.h"
//#include "ns3/common-module.h"
//#include "ns3/helper-module.h"
//#include "ns3/node-module.h"
//#include "ns3/simulator-module.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("PeriodicDumbBell");

//static const uint32_t totalTxBytes = 2000000;
//static uint32_t currentTxBytes = 0;
// Perform series of 1040 byte writes (this is a multiple of 26 since
// we want to detect data splicing in the output stream)
static const uint32_t writeSize = 65536;
uint8_t data[writeSize];

static const float stopTime = 10; // seconds

// These are for starting the writing process, and handling the sending
// socket's notification upcalls (events).  These two together more or less
// implement a sending "Application", although not a proper ns3::Application
// subclass.

void StartFlow(Ptr<Socket>, Ipv4Address, uint16_t);

void SendPackets(Ptr<Socket>);

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);
  uint32_t servPort = 5000;

  // Default Arguments
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue
(1380));
  // Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (131072));  // it is already 128 KiB
  Config::SetDefault ("ns3::TcpSocket::SlowStartThreshold",
UintegerValue (16777216)); //16 MB

  // Helpers for the leaf nodes and the Routers
  // Assuming server and client have same capacities this can be extended later
  PointToPointHelper pointToPointRouter;
  pointToPointRouter.SetDeviceAttribute  ("DataRate", StringValue
("10Mbps"));
  pointToPointRouter.SetChannelAttribute ("Delay", StringValue
("1ms"));
  PointToPointHelper pointToPointLeaf;
  pointToPointLeaf.SetDeviceAttribute    ("DataRate", StringValue
("10Mbps"));
  pointToPointLeaf.SetChannelAttribute   ("Delay", StringValue
("1ms"));
  uint32_t nLeaf = 5; // number of leaf nodes for the dumbBell

  PointToPointDumbbellHelper dBell(nLeaf, pointToPointLeaf,
                                   nLeaf, pointToPointLeaf,
                                   pointToPointRouter);
  // Now add ip/tcp stack to all nodes and use the linux tcp library
  InternetStackHelper stack;
  stack.SetTcp ("ns3::NscTcpL4Protocol","Library",
StringValue("liblinux2.6.26.so"));
  dBell.InstallStack (stack); // note first install the stack then set attribute

  // Now Assign the IPaddress to the devices
  dBell.AssignIpv4Addresses (Ipv4AddressHelper ("10.1.1.0",
"255.255.255.240"),
                             Ipv4AddressHelper ("10.2.1.0",
"255.255.255.240"),
                             Ipv4AddressHelper ("10.3.1.0",
"255.255.255.240"));

  // and setup ip routing tables to get total ip-level connectivity.
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Ptr<Socket> localSocket[nLeaf];
  PacketSinkHelper sink ("ns3::TcpSocketFactory",
                         InetSocketAddress(Ipv4Address::GetAny (),
servPort));
  ApplicationContainer clientApps;

  for(uint32_t i=0;i<nLeaf;i=i+1)
    {
      clientApps.Add(sink.Install(dBell.GetRight (i)));
      localSocket[i] = Socket::CreateSocket (dBell.GetLeft(i),
TcpSocketFactory::GetTypeId ());
      localSocket[i]->Bind ();
    }

  // Create and bind the socket...
  for (uint32_t i=0; i<nLeaf;i++)
    {
      Simulator::ScheduleNow (&StartFlow, localSocket[i],
                              dBell.GetRightIpv4Address(i),
                              servPort);

    }
  // Enable tracing
  AsciiTraceHelper ascii;
  pointToPointRouter.EnableAsciiAll(ascii.CreateFileStream ("dumbell-transfer.tr"));
  // Start the simulation
  Simulator::Stop (Seconds(stopTime+5.0));
  Simulator::Run ();
  Simulator::Destroy ();
}

//begin implementation of sending "Application"
void
StartFlow(Ptr<Socket> localSocket,
            Ipv4Address servAddress,
            uint16_t servPort)
{
  localSocket->Connect (InetSocketAddress (servAddress, servPort));//connect
  Simulator::ScheduleNow (&SendPackets, localSocket);
}

void
SendPackets(Ptr<Socket> localSocket)
{
  //int amountSent = localSocket->Send (data, writeSize, 0);
  //  std::cout<<amountSent<<localSocket<<std::endl;
  localSocket->Send (data, writeSize, 0);
  Simulator::Schedule (Seconds(1), &SendPackets, localSocket);
}
