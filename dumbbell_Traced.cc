


/*

    1. Create a simple dumbbell topology, two client Node1 and Node2 on
    the left side of the dumbbell and server nodes Node3 and Node4 on the
    right side of the dumbbell. Let Node5 and Node6 form the bridge of the
    dumbbell. Use point to point links.

    2. Install a TCP socket instance on Node1 that will connect to Node3.

    3. Install a UDP socket instance on Node2 that will connect to Node4.

    4. Start the TCP application at time 1s.

    5. Start the UDP application at time 20s at rate Rate1 such that it clogs
    half the dumbbell bridge's link capacity.

    6. Increase the UDP application's rate at time 30s to rate Rate2
    such that it clogs the whole of the dumbbell bridge's capacity.

    7. Use the ns-3 tracing mechanism to record changes in congestion window
    size of the TCP instance over time. Use gnuplot/matplotlib to visualise plots of cwnd vs time.

    8. Mark points of fast recovery and slow start in the graphs.

    9. Perform the above experiment for TCP variants Tahoe, Reno and New Reno,
    all of which are available with ns-3.

*/

// Network topology
//
//       n0 ---+        +--- n10
//       n1 ---|        |
//       n2 ---n8 ----- n9
//       :     |        |
//       n6 ---|
//       n7 ---+        +--- n11 // Cut out for now
//
// - All links are P2P with 500kb/s and 2ms
// - TCP flow form n0 to n10
// - TCP flow from n1 to n10
// - UDP flow from n1 to n3

#include <fstream>
#include <string>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
//#include "ns3/point-to-point-dumbbell.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Lab2");

class MyApp : public Application
{
public:

  MyApp ();
  virtual ~MyApp();

  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);
  void ChangeRate(DataRate newrate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  DataRate        m_dataRate;
  EventId         m_sendEvent;
  bool            m_running;
  uint32_t        m_packetsSent;
};

MyApp::MyApp ()
  : m_socket (0),
    m_peer (),
    m_packetSize (0),
    m_nPackets (0),
    m_dataRate (0),
    m_sendEvent (),
    m_running (false),
    m_packetsSent (0)
{
}

MyApp::~MyApp()
{
  m_socket = 0;
}

void
MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_dataRate = dataRate;
}

void
MyApp::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;
  m_socket->Bind ();
  m_socket->Connect (m_peer);
  SendPacket ();
}

void
MyApp::StopApplication (void)
{
  m_running = false;

  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }

  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
MyApp::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  if (++m_packetsSent < m_nPackets)
    {
      ScheduleTx ();
    }
}

void
MyApp::ScheduleTx (void)
{
  if (m_running)
    {
      Time tNext (Seconds (m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
      m_sendEvent = Simulator::Schedule (tNext, &MyApp::SendPacket, this);
    }
}

void
MyApp::ChangeRate(DataRate newrate)
{
   m_dataRate = newrate;
   return;
}

static void
CwndChange (uint32_t oldCwnd, uint32_t newCwnd)
{
  std::cout << Simulator::Now ().GetSeconds () << "\t" << newCwnd <<"\n";
}

void
IncRate (Ptr<MyApp> app, DataRate rate)
{
	app->ChangeRate(rate);
    return;
}


int cCnt = 1;
int tCnt = 1;
static void
CwndChange2 (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
	//int i = 0;
  NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "\t" << newCwnd);
  *stream->GetStream () << tCnt << "\t" << Simulator::Now ().GetSeconds () << "\t" << oldCwnd << "\t" << newCwnd << std::endl;
  if (cCnt%2 == 0)
	  tCnt++;
  cCnt++;
}


static void
TxTrace (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> p)
{
  NS_LOG_UNCOND ("Tx at " << Simulator::Now ().GetSeconds ());
  *stream->GetStream () << Simulator::Now ().GetSeconds ()<< std::endl;
}


static void
RxTrace (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> p)
{
  NS_LOG_UNCOND ("Rx at " << Simulator::Now ().GetSeconds ());
  *stream->GetStream () << tCnt++ << "\t" << Simulator::Now ().GetSeconds ()<< std::endl;
}


int main (int argc, char *argv[])
{
  std::string lat = "2ms";
  std::string rate = "500kb/s"; // P2P link
  bool enableFlowMonitor = false;


  CommandLine cmd;
  cmd.AddValue ("latency", "P2P link Latency in miliseconds", lat);
  cmd.AddValue ("rate", "P2P data rate in bps", rate);
  cmd.AddValue ("EnableMonitor", "Enable Flow Monitor", enableFlowMonitor);

  cmd.Parse (argc, argv);

//
// Explicitly create the nodes required by the topology (shown above).
//
  NS_LOG_INFO ("Create nodes.");
  NodeContainer c; // ALL Nodes
  c.Create(11);

  NodeContainer n0n8 = NodeContainer (c.Get (0), c.Get (8));
  NodeContainer n1n8 = NodeContainer (c.Get (1), c.Get (8));
  NodeContainer n2n8 = NodeContainer (c.Get (2), c.Get (8));
  NodeContainer n3n8 = NodeContainer (c.Get (3), c.Get (8));
  NodeContainer n4n8 = NodeContainer (c.Get (4), c.Get (8));
  NodeContainer n5n8 = NodeContainer (c.Get (5), c.Get (8));
  NodeContainer n6n8 = NodeContainer (c.Get (6), c.Get (8));
  NodeContainer n7n8 = NodeContainer (c.Get (7), c.Get (8));
  NodeContainer n10n9 = NodeContainer (c.Get (10), c.Get (9));
  NodeContainer n8n9 = NodeContainer (c.Get (8), c.Get (9));


  /*
   * SET TCP TYPE:
  */
  //Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpReno"));

//
// Install Internet Stack
//
  InternetStackHelper internet;
  internet.Install (c);

  // We create the channels first without any IP addressing information
  NS_LOG_INFO ("Create channels.");
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue (rate));
  p2p.SetChannelAttribute ("Delay", StringValue (lat));
  NetDeviceContainer d0d8 = p2p.Install (n0n8);
  NetDeviceContainer d1d8 = p2p.Install (n1n8);
  NetDeviceContainer d3d8 = p2p.Install (n3n8);
  NetDeviceContainer d4d8 = p2p.Install (n4n8);
  NetDeviceContainer d5d8 = p2p.Install (n5n8);
  NetDeviceContainer d6d8 = p2p.Install (n6n8);
  NetDeviceContainer d7d8 = p2p.Install (n7n8);

  NetDeviceContainer d8d9 = p2p.Install (n8n9);
  NetDeviceContainer d10d9 = p2p.Install (n10n9);
  //NetDeviceContainer d3d5 = p2p.Install (n3n5);


  Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
  em->SetAttribute ("ErrorRate", DoubleValue (0.00001));
  d10d9.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));

  //Simulator::Schedule (Seconds (2), &RateErrorModel::SetRate, em, 0.00002);
  for (int i=10; i<50;) {
	  Simulator::Schedule (Seconds (i), &RateErrorModel::SetRate, em, 1);
	  i+=10;
	  Simulator::Schedule (Seconds (i), &RateErrorModel::SetRate, em, 0.00001);
	  i+=10;
  }
  //Simulator::Schedule (Seconds (10), &RateErrorModel::SetRate, em, 0.00001);
  //Simulator::Schedule (Seconds (15), &RateErrorModel::SetRate, em, 1);

    // Later, we add IP addresses.
  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i8 = ipv4.Assign (d0d8);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i8 = ipv4.Assign (d1d8);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i8i9 = ipv4.Assign (d8d9);

  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i10i9 = ipv4.Assign (d10d9);

  ipv4.SetBase ("10.1.5.0", "255.255.255.0");
  //Ipv4InterfaceContainer i3i5 = ipv4.Assign (d3d5);

  NS_LOG_INFO ("Enable static global routing.");
  //
  // Turn on global static routing so we can actually be routed across the network.
  //
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();


  NS_LOG_INFO ("Create Applications.");

  // TCP connection from N0 to N10

  uint16_t sinkPort = 8080;
  Address sinkAddress (InetSocketAddress (i10i9.GetAddress (0), sinkPort)); // interface of n10
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install (c.Get (10)); //n10 as sink
  sinkApps.Start (Seconds (0.));
  sinkApps.Stop (Seconds (50.));

  Ptr<Socket> ns3TcpSocket0 = Socket::CreateSocket (c.Get (0), TcpSocketFactory::GetTypeId ()); //source at n0
  Ptr<Socket> ns3TcpSocket1 = Socket::CreateSocket (c.Get (1), TcpSocketFactory::GetTypeId ()); //source at n1



  // Trace Congestion window
  ns3TcpSocket0->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndChange));
  ns3TcpSocket1->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndChange));

  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream ("dBell1_10secOnOff.cwnd");
  ns3TcpSocket0->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (&CwndChange2, stream));
  Ptr<OutputStreamWrapper> stream2 = asciiTraceHelper.CreateFileStream ("dBell2_10secOnOff.cwnd");
  ns3TcpSocket1->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (&CwndChange2, stream2));

  Ptr<OutputStreamWrapper> streamN0 = asciiTraceHelper.CreateFileStream ("N0_Tx");
  d0d8.Get (0)->TraceConnectWithoutContext ("PhyTxBegin", MakeBoundCallback (&TxTrace, streamN0));
  Ptr<OutputStreamWrapper> streamN1 = asciiTraceHelper.CreateFileStream ("N1_Tx");
  d1d8.Get (0)->TraceConnectWithoutContext ("PhyTxBegin", MakeBoundCallback (&TxTrace, streamN1));
  Ptr<OutputStreamWrapper> streamN10 = asciiTraceHelper.CreateFileStream ("N10_Rx");
  d10d9.Get (1)->TraceConnectWithoutContext ("PhyRxEnd", MakeBoundCallback (&RxTrace, streamN10));



  // Create TCP application at n0, n1
  Ptr<MyApp> app = CreateObject<MyApp> ();
  Ptr<MyApp> app2 = CreateObject<MyApp> ();
  app->Setup (ns3TcpSocket0, sinkAddress, 1040, 10000000, DataRate ("250Kbps"));
  app2->Setup (ns3TcpSocket1, sinkAddress, 1040, 10000000, DataRate ("10000Kbps"));
  c.Get (0)->AddApplication (app);
  c.Get (1)->AddApplication (app2);
  app->SetStartTime (Seconds (1.));
  app->SetStopTime (Seconds (50.));
  app2->SetStartTime (Seconds (1.));
  app2->SetStopTime (Seconds (50.));



  // UDP connection from N1 to N3

//  uint16_t sinkPort2 = 6;
//  Address sinkAddress2 (InetSocketAddress (i3i5.GetAddress (0), sinkPort2)); // interface of n3
// PacketSinkHelper packetSinkHelper2 ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort2));
//  ApplicationContainer sinkApps2 = packetSinkHelper2.Install (c.Get (3)); //n3 as sink
//  sinkApps2.Start (Seconds (0.));
//  sinkApps2.Stop (Seconds (50.));

//  Ptr<Socket> ns3UdpSocket = Socket::CreateSocket (c.Get (1), UdpSocketFactory::GetTypeId ()); //source at n1

  // Create UDP application at n1
//  Ptr<MyApp> app2 = CreateObject<MyApp> ();
//  app2->Setup (ns3UdpSocket, sinkAddress2, 1040, 100000, DataRate ("250Kbps"));
//  c.Get (1)->AddApplication (app2);
//  app2->SetStartTime (Seconds (20.));
//  app2->SetStopTime (Seconds (100.));

// Increase UDP Rate
//  Simulator::Schedule (Seconds(30.0), &IncRate, app2, DataRate("500kbps"));



  Ptr<FlowMonitor> flowmon;

      FlowMonitorHelper flowmonHelper;
      flowmon = flowmonHelper.InstallAll ();


//
// Now, do the actual simulation.
//
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds(50.0));
  Simulator::Run ();

  flowmon->CheckForLostPackets ();
  flowmon->SerializeToXmlFile("dBellTraced.flowmon", true, true);

  std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats ();

	uint32_t txPacketsum = 0;
	uint32_t rxPacketsum = 0;
	uint32_t DropPacketsum = 0;
	uint32_t LostPacketsum = 0;
	double Delaysum = 0;

	for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i =stats.begin (); i != stats.end (); ++i)
	{
	   txPacketsum += i->second.txPackets;
	   rxPacketsum += i->second.rxPackets;
	   LostPacketsum += i->second.lostPackets;
	   DropPacketsum += i->second.packetsDropped.size();
	   Delaysum += i->second.delaySum.GetSeconds();
	}
	std::cout << "  All Tx Packets: " << txPacketsum << "\n";
	std::cout << "  All Rx Packets: " << rxPacketsum << "\n";
	std::cout << "  All Delay: " << Delaysum / txPacketsum <<"\n";
	std::cout << "  All Lost Packets: " << LostPacketsum << "\n";
	std::cout << "  All Drop Packets: " << DropPacketsum << "\n";
	std::cout << "  Packets Delivery Ratio: " << ((rxPacketsum *100) /txPacketsum) << "%" << "\n";
	std::cout << "  Packets Lost Ratio: " << ((LostPacketsum *100) /txPacketsum) << "%" << "\n";

  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}
