#include "SEProducer.hpp"
#include "GenericConsumer.hpp"
#include "TopoProcConsumer.hpp"
#include "SensorDefConsumer.hpp"
#include "SELoopConsumer.hpp"

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

		// --------------------------------------------------------------------
		// FOR DEMO ONLY -- REQUEST THE LIST OF MODELS
		// --------------------------------------------------------------------
		
		cout << "\nPress ENTER to request the list of models:";
		while ( cin.get() != '\n');

		// Set up the model query consumer
		string mqTopic = "goss.gridappsd.se.response."+simid;
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


		// --------------------------------------------------------------------
		// TOPOLOGY PROCESSOR
		// --------------------------------------------------------------------

		cout << "\nPress ENTER to send the Y-bus request:";
		while ( cin.get() != '\n' );

		// Set up the ybus consumer
		string ybusTopic = "goss.gridappsd.se.response."+simid;
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
	
		// Report
		cout << "\nPress ENTER to analyze the Y-bus:";
		while ( cin.get() != '\n' );
	
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
		for ( auto itr = nodens.begin() ; itr != nodens.end() ; itr++ ) {
			xV[nodem[*itr]] = vnom;
			xT[nodem[*itr]] = 0;
		}
		int xqty = xV.size() + xT.size();
		if ( xqty != 2*numns ) throw "x initialization failed";
	
		// --------------------------------------------------------------------
		// SENSOR INITILIZER
		// --------------------------------------------------------------------
		
		cout << "\nPress ENTER to send the sensor request:";
		while ( cin.get() != '\n' );

		// Set up the sensors consumer
		string sensTopic = "goss.gridappsd.se.response."+simid;
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
		uint numms; 	// number of sensors
		SLIST mns;		// sensor name [list of strings]
		SSMAP mts;		// sensor type [sn->str]
		SDMAP msigs;	// sensor sigma: standard deviation [sn->double]
		SSMAP mnd1s;	// point node or from node for flow sensors [sn->str]
		SSMAP mnd2s;	// point node or to node for flow sensors [sn->str]
		SDMAP mvals;	// value of the latest measurement [sn->double]
		
		// Wait for sensor initializer and retrieve sensors
		sensConsumerThread.join();
		sensConsumer.fillSens(numms,mns,mts,msigs,mnd1s,mnd2s,mvals);
		sensConsumer.close();
		
		int zqty = mns.size();
		
//		// DEBUG outputs
//		cout << '\n';
//		for ( auto itr = mns.begin() ; itr != mns.end() ; itr++ ) {
//			cout << *itr << '\n';
//		}
		
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
		
		
/*
		// --------------------------------------------------------------------
		// LISTEN FOR MEASUREMENTS
		// --------------------------------------------------------------------

		// ideally we want to compute an estimate on a thread at intervals and
		// collect measurements in the meantime
		int done = 1;
		while ( !done ) {
			
		}

		// for now, use one thread that listens, estimates, and publishes
		//string measTopic = "tmpMeasurementTopic";
		//string measTopic = "goss.gridappsd.simulation.output."+simid;
		string measTopic = "goss.gridappsd.fncs.output";
		SELoopConsumer loopConsumer(brokerURI,username,password,measTopic,"topic"); // probably need to pass state to this
		Thread loopConsumerThread(&loopConsumer);
		loopConsumerThread.start();		// execute loopConsumer.run()
		loopConsumer.waitUntilReady();	// wait for the latch to release

		// the rest of the app executes in loopConsumer.onMessage() ???
		// do we need to recover the updated states?


//		// Spoof measurement messages
//		string fakeMeasurements = "FAKE_MEASUREMENTS";
//		SEProducer measurementSpoofer(brokerURI,username,password,measTopic,"topic");
//		for (int ii = 0 ; ii < 10 ; ii++ )
//			measurementSpoofer.send(to_string(ii)+": "+fakeMeasurements);
//		measurementSpoofer.send((string)"stop");
//		measurementSpoofer.close();

		// we can wait for the estimator to exit:
		loopConsumerThread.join(); loopConsumer.close();

*/

		cout << "\nPress ENTER to complete the Demo:";
		while ( cin.get() != '\n');
		cout << "\tDemo Complete.\n\n";
		
		// now we're done
		return 0;

	} catch (...) {
		std::cerr << "Error: Unhandled AMQ Exception\n";
		throw NULL;
	}
	
}
