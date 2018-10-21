#include "SEProducer.hpp"
#include "GenericConsumer.hpp"
#include "TopoProcConsumer.hpp"
#include "SensorDefConsumer.hpp"
#include "SELoopConsumer.hpp"

#include "SensorArray.hpp"

#include <iostream>

// standard data types
#include <string>
#include <complex>
#include <list>
#include <unordered_map>

// macro for unsigned int
#define uint unsigned int

// Store node names in a linked list and hash node name to their position
// Iterate over the linked list to access all nodes or states
// Note that positions are one-indexed
// Store sensor names in a linked list and hash node names to various params
#define SLIST std::list<std::string>
#define SIMAP std::unordered_map<std::string,unsigned int>
#define SDMAP std::unordered_map<std::string,double>
#define SSMAP std::unordered_map<std::string,std::string>

// Hash address (i,j) to the index of a sparse matrix vector
#define ICMAP std::unordered_map<unsigned int,std::complex<double>>
#define IMMAP std::unordered_map<unsigned int,ICMAP>

// Store x and z in a list one-indexed by position
#define IDMAP std::unordered_map<unsigned int,double>

int main(int argc, char** argv){
	
	// ------------------------------------------------------------------------
	// GET RUNTME ARGS
	// ------------------------------------------------------------------------
	std::string simid,addr,port,username,password;
	
	// Required first input: simulation ID
	if ( argc > 1 ) {
		simid = argv[1];
	} else {
		std::cerr << "Usage: " << *argv
			<< " simid ipaddr port username password\n";
		// return 0;
	}
	
	// Host address
	if ( argc > 2 ) addr = argv[2];
	else addr = "127.0.0.1";
	
	// Host port
	if ( argc > 3 ) port = argv[3];
	else port = "61616";
	
	// Username
	if ( argc > 4 ) username = argv[4];
	else username = "system";
	
	// Password
	if ( argc > 5 ) password = argv[5];
	else password = "manager";
	
	// Check for extraneous inputs
	if (argc > 6 ) {
		std::cerr << "Unrecognized inpt: " << argv[6] << '\n';
		std::cerr << "Usage: " << *argv
			<< " simid ipaddr port username password\n";
		return 0;
	}
	

	// ------------------------------------------------------------------------
	// START THE AMQ INTERFACE
	// ------------------------------------------------------------------------

	string brokerURI = "tcp://"+addr+':'+port;
	//string brokerURI = "failover:(tcp://WE33461.pnl.gov:61616)";

	try {
		activemq::library::ActiveMQCPP::initializeLibrary();


		// --------------------------------------------------------------------
		// SET UP THE MESSAGE LOGGER
		// --------------------------------------------------------------------

		// string logtopic = "goss.gridappsd.simulation.log."+simid;
		// SEProducer logger(brokerURI,username,password,logtopic,"queue");
/*
		// --------------------------------------------------------------------
		// FOR DEMO ONLY -- REQUEST THE LIST OF MODELS
		// --------------------------------------------------------------------
		
		// Set up the model query consumer
		string mqTopic = "goss.gridappsd.se.response."+simid+".modquery";
		GenericConsumer mqConsumer(brokerURI,username,password,mqTopic,"queue");
		Thread mqConsumerThread(&mqConsumer);
		mqConsumerThread.start();		// execute modelQueryConsumer.run()
		mqConsumer.waitUntilReady(); 	// wait for latch release

		// Set up the model query producer to request the model query
		string mqRequestTopic = "goss.gridappsd.process.request.data.powergridmodel";
		string mqRequestText = 
			"{\"requestType\":\"QUERY_MODEL_INFO\",\"resultFormat\":\"JSON\"}";
		SEProducer mqRequester(brokerURI,username,password,mqRequestTopic,"queue");
		mqRequester.send(mqRequestText,mqTopic);
		mqRequester.close();
		mqConsumerThread.join();
		mqConsumer.close();
*/

		// --------------------------------------------------------------------
		// TOPOLOGY PROCESSOR
		// --------------------------------------------------------------------

		// Set up the ybus consumer
		string ybusTopic = "goss.gridappsd.se.response."+simid+".ybus";
		TopoProcConsumer ybusConsumer(brokerURI,username,password,ybusTopic,"queue");
		Thread ybusConsumerThread(&ybusConsumer);
		ybusConsumerThread.start();		// execute ybusConsumer.run()
		ybusConsumer.waitUntilReady();	// wait for latch release

		// Set up the producer to request the ybus
		string ybusRequestTopic = "goss.gridappsd.process.request.config";
		string ybusRequestText = 
			"{\"configurationType\":\"YBus Export\",\"parameters\":{\"simulation_id\":\"" 
			+ simid + "\"}}";
		SEProducer ybusRequester(brokerURI,username,password,ybusRequestTopic,"queue");
		ybusRequester.send(ybusRequestText,ybusTopic);
		ybusRequester.close();


		/*
		// Spoof message to the consumer
		string ybusSpoofedResponse = 
			R"({
				"data":"{
					"yParseFilePath":"/tmp/gridappsd_tmp/1129698954/base_ysparse.csv",
					"nodeListFilePath":"/tmp/gridappsd_tmp/1129698954/base_nodelist.csv",
					"summaryFilePath":"/tmp/gridappsd_tmp/1129698954/base_summary.csv"
				}",
				"responseComplete":true,
				"id":"6429475f-fed7-4b9e-9f66-21077a322bf6"
			})";
		SEProducer ybusSpoofer(brokerURI,username,password,ybusTopic,"topic");
		ybusSpoofer.send(ybusSpoofedResponse);
		ybusSpoofer.close();
		*/

		
		// Initialize topology
		uint numns;		// number of nodes
		SLIST nodens;	// list of node names
		SIMAP nodem;	// map from node name to unit-indexed position
		IMMAP Y;		// double map from indices to complex admittance
		// G, B, g, and b are derived from Y:
		//	-- Gij = std::real(Y[i][j]);
		//	-- Bij = std::imag(Y[i][j]);
		//	-- gij = std::real(-1.0*Y[i][j]);
		//	-- bij = std::imag(-1.0*Y[i][j]);
		
		// Wait for topological processor and retrieve topology

		ybusConsumerThread.join();
		ybusConsumer.fillTopo(numns,nodens,nodem,Y);
		ybusConsumer.close();
		
//		// DEBUG outputs
//		// print back nodens and their positiions from nodem
//		for ( auto itr = nodens.begin() ; itr != nodens.end() ; itr++ ) 
//			cout << "Node |" + *itr + "| -> " + to_string(nodem[*itr]) + '\n';
//		// print select elements of Y
//		// std::cout << "Y[1][1] = " << Y[1][1] << '\n';
//		// std::cout << "Y[35][36] = " << Y[35][36] << '\n';
//		// list the populated index pairs in Y
//		for ( auto itr=Y.begin() ; itr!=Y.end() ; itr++ ) {
//			int i = std::get<0>(*itr);
//			cout << "coulumns in row " + to_string(i) + ":\n\t";
//			for ( auto jtr=Y[i].begin() ; jtr!=Y[i].end() ; jtr++ ) {
//				int j = std::get<0>(*jtr);
//				cout << to_string(j) + '\t';
//			}
//			cout << '\n';
//		}
	
		// Report the number of nodes
		unsigned int nodectr = 0;
		for ( auto itr = nodens.begin() ; itr != nodens.end() ; itr++ ) nodectr++;
		cout << "\tNumber of nodes loaded: " << nodectr << '\n';

		// Report the number of entries in Y
		unsigned int ybusctr = 0;
		for ( auto itr = Y.begin() ; itr != Y.end() ; itr++ ) {
			int i = std::get<0>(*itr);
			for ( auto jtr = Y[i].begin() ; jtr != Y[i].end() ; jtr++ ) ybusctr++;
		}
		cout << "\tNumber of Ybus entries loaded: " << ybusctr << '\n';


		// INITIALIZE THE STATE VECTOR
		double vnom = 0.0;	// get this from the CIM?
		IDMAP xV;	// container for voltage magnitude states
		IDMAP xT;	// container for voltage angle states
		for ( auto& nodename : nodens ) {
			xV[nodem[nodename]] = vnom;
			xT[nodem[nodename]] = 0;
		}
		int xqty = xV.size() + xT.size();
		if ( xqty != 2*numns ) throw "x initialization failed";
	
		// --------------------------------------------------------------------
		// SENSOR INITILIZER
		// --------------------------------------------------------------------
		
		// Set up the sensors consumer
		string sensTopic = "goss.gridappsd.se.response."+simid+".cimdict";
		SensorDefConsumer sensConsumer(brokerURI,username,password,sensTopic,"queue");
		Thread sensConsumerThread(&sensConsumer);
		sensConsumerThread.start();		// execute sensConsumer.run()
		sensConsumer.waitUntilReady();	// wait for latch release

		// Set up the producer to request sensor data
		string sensRequestTopic = "goss.gridappsd.process.request.config";
		string sensRequestText = 
			"{\"configurationType\":\"CIM Dictionary\",\"parameters\":{\"simulation_id\":\""
			+ simid + "\"}}";
		SEProducer sensRequester(brokerURI,username,password,sensRequestTopic,"queue");
		sensRequester.send(sensRequestText,sensTopic);
		sensRequester.close();

		// Initialize sensors
		SensorArray zary;
//		uint numms; 	// number of sensors
//		SLIST mns;		// sensor name [list of strings]
//		SSMAP mts;		// sensor type [sn->str]
//		SDMAP msigs;	// sensor sigma: standard deviation [sn->double]
//		SSMAP mnd1s;	// point node or from node for flow sensors [sn->str]
//		SSMAP mnd2s;	// point node or to node for flow sensors [sn->str]
//		SDMAP mvals;	// value of the latest measurement [sn->double]
		
		// Wait for sensor initializer and retrieve sensors
		sensConsumerThread.join();
//		sensConsumer.fillSens(numms,mns,mts,msigs,mnd1s,mnd2s,mvals);
		sensConsumer.fillSens(zary);
		sensConsumer.close();
		
//		// Print the nodes associated with each sensor
//		for ( auto& mn : mns ) cout << mnd1s[mn] + '\n';

		// --------------------------------------
		// COMPARE THE SENSOR NODES TO YBUS NODES
		// --------------------------------------
		unordered_map<string,int> dss_node_found;
		vector<string> unmatched_measurement_nodes;
		int map_match_ctr = 0;
		for ( auto& measurement_name : zary.zids ) {
			// get the node associated with each measurement
			string measurement_node = zary.znode1s[measurement_name];
			try {
				// check whether the measurement node exists in opendss
				int measurement_node_index = nodem.at(measurement_node);
//				cout << measurement_node << " maps to index " 
//						<< measurement_node_index << '\n';
				dss_node_found[measurement_node] ++;
				map_match_ctr++;
			} catch(...) {
				unmatched_measurement_nodes.push_back(measurement_node);
			}
		}

		// report the number of matches
		cout << "\n\nNumber of measurement nodes successfully mapped to OpenDSS nodes: "
				<< map_match_ctr << '\n';

		// list the measurement nodes that are not successfully mapped
		cout << "\n\nMeasurement nodes that do not exist in Ybus:\n";
		for ( auto& unmatched_measurement_node : unmatched_measurement_nodes ) {
			cout << "\t" + unmatched_measurement_node + "\n";
		}

		// list Ybus nodes that have no measurement
		int measured_ctr = 0;
		cout << "\n\nYbus nodes that do not have a measurement:\n";
		for ( auto& ybus_node : nodens ) {
			if ( dss_node_found[ybus_node] ) measured_ctr++;
			else cout << "\t" + ybus_node + "\n";
		}

		// report the number of ybus nodes with measurements
		cout << "\n\n" << measured_ctr << " Ybus nodes have measurements\n";

		// list Ybus nodes with more than one measurement
		int multi_measurement_ctr = 0;
		int multi_meas_aggr = 0;
//		cout << "\n\nYbus nodes with more than one measurement:\n";
		for ( auto& ybus_node: nodens ) {
			if ( dss_node_found[ybus_node] > 1 ) {
//				cout << ybus_node << " has " 
//						<< dss_node_found[ybus_node] << " measurements\n";
				multi_measurement_ctr++;
				multi_meas_aggr += dss_node_found[ybus_node];
			}
		}

		// report the number of ybus nodes with multiple measurements
		cout << "\n\n" << multi_measurement_ctr 
				<< " Ybus nodes have multiple measurements for a total of "
				<< multi_meas_aggr << " measurements\n";


		
		
		// --------------------------------------------------------------------
		// Build the measurement function h(x) and its Jacobian J(x)
		// --------------------------------------------------------------------
		
		/*
		// INITIALIZE THE MEASUREMENT FUNCTION h(x)
		enum hx_t {
				Pij ,
				Qij ,
				Pi ,
				Qi };
		DVEC hx;
		std::vector<hx_t> thx;
		std::vector<uint> hxi;
		std::vector<std::vector<uint>> hxj;
	for ( auto itr = sns.begin(); itr != sns.end() ; itr++ ) {
			// for each measurement:
			hx.push_back(svals[*itr]);			// set the initial value
			switch(sts[*itr]) {					// set the measurement type
				case("Pij"): thx.push_back(Pij); break;
				case("Qij"): thx.push_back(Qij); break;
				case("Pi"): thx.push_back(Pi); break;
				case("Qi"): thx.push_back(Qi); break;
				default: throw("unrecognized sensor type"); }
			uint i = snd1s[*itr]];
			hxi.push_back(nodem[i]);						// set node i
			if ( sts[*itr] == "Pij" || sts[*itr] == "Qij" )	// set flow j
				hxj.push_back( (vector)(nodem[snd2s[*itr]]) );
			else {											// set injection js
				// locate all adjacent nodes
				std::vector<uint>> tmpjs;
				for ( auto jtr=Y[i].begin() ; jtr!=Y[i].end() ; jtr++ ) {
					uint j = std::get<0>(*jtr);
					if ( j != i ) tmpjs.push_back(j);
				}
				hxj.push_back(tmpjs);
			}
		}
		*/
		
		/*
		// INITIALIZE THE MEASUREMENT FUNCTION JACOBIAN J(x)
		enum Jx_t {
				dPijdVi , dPijdVj , dPijdTi , dPijdTj , 	
				dQijdVi , dQijdVj , dQijdTi , dQijdTj , 
				dPidVi  , dPidVj  , dPidTi  , dPidTj  ,
				dQidVi  , dQidVj  , dQidTi  , dQidTj  };
		DVEC Jx;
		std::vector<Jx_t> tJx;
		// std::vector<uint> Jxi;
		// std::vector<std::vector<uint>> Jxj;
		for ( int ii = 0 ; ii < zqty ; ii++ ) {
			// for each measurement function:
			for ( int jj = 0 ; jj < xqty ; jj ++ ) {
				// establish the derivetive with respect to each state
				
				// Determine whether the derivative is non-zero
				
				
				// Jx.append(initial value)
				Jx.push_back(0);
				// tJx.append(type [Jx_t])
				switch(thx[ii]) {
					case(Pij):					// power flow measurement
						if ( jj < xqty/2 ) {	// voltage state
							if ( ii ==
								tJx.push_back(dPijdVi)
					case(Qij):
					case(Pi):
					case(Qi):
				
				// i???
				// j???
				// rows correspond to measurements
				// columns correspond to derivatives with respect to states
			}
		}
		*/
		
		

		// --------------------------------------------------------------------
		// LISTEN FOR MEASUREMENTS
		// --------------------------------------------------------------------

		// ideally we want to compute an estimate on a thread at intervals and
		// collect measurements in the meantime
/*	
		// PUT THIS INSIDE PROCESS
		"goss.gridappsd.state-estimator.output"+simid
		SEProducer ybusRequester(brokerURI,username,password,SETopic,"topic");
*/
		// measurements come from the simulation output
		string simoutTopic = "goss.gridappsd.simulation.output."+simid;
		SELoopConsumer loopConsumer(brokerURI,username,password,simoutTopic,"topic",simid);
		Thread loopConsumerThread(&loopConsumer);
		loopConsumerThread.start();	// execute loopConsumer.run()
		loopConsumer.waitUntilReady();	// wait for the startup latch release
		
		cout << "\nListening for simulation output on "+simoutTopic+'\n';
		
//		// I'm not sure of the right way to synchronously access the message content
//		// For now, all processing will be done inside the consumer.
//		//   - messages will be dropped if processing is not complete in time.
//		//	 - at a minimum, the algorithm cares about the time between measurements
//		while ( loopConsumer.doneLatch.getCount() ) {
//			loopConsumer.waitForData();		// don't process until data write is complete
//
//		}
		
		// wait for the estimator to exit:
		loopConsumerThread.join(); loopConsumer.close();

		// now we're done
		return 0;

	} catch (...) {
		std::cerr << "Error: Unhandled AMQ Exception\n";
		throw NULL;
	}
	
}
