#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string>
#include <vector>
#include <time.h>
#include <sstream>

//soubor obsahujici funkce pro vyuziti udt prenosu
#include "udt.h"

//konstanty pro nastaveni vlastnosti programu
#define PACK_DATA_SIZE 300
#define RTT 1000

using namespace std;

void usage(){
		cerr << "Program rdtclient" << endl;
		cerr << "Barbora Skrivankova, xskriv01@stud.fit.vutbr.cz" << endl;
		cerr << "Pouziti: ./rdtclient -s source_port -d dest_port" << endl;
}

string convertInt(int number)
{
   stringstream ss;
   ss << number;
   return ss.str();
}

int getPackId(string packet){
	unsigned pos, endpos;
	int ret = 0;

	pos = packet.find("sn");
	endpos = packet.find("ack");
	if((pos != string::npos) && (endpos != string::npos)){
		ret = atoi(packet.substr(pos + 4, endpos - 2).c_str());
	}
	return ret;
}

int getTack(string packet){
	unsigned pos, endpos;
	int ret = 0;

	pos = packet.find("tack");
	endpos = packet.substr(pos).find(">");
	if(pos != string::npos){
		ret = atoi(packet.substr(pos + 6, endpos-1).c_str());
	}
	return ret;
}

string createXML(int sn, string data, int window_size, int time){
	string value = "<rdt-segment id=\"xskriv01\">\n<header sn=\"";
	value.append(convertInt(sn));
	value.append( "\" ack=\"potvrzeni\" win=\"");
	value.append(convertInt(window_size));
	value.append("\" tack=\"");
	value.append(convertInt(time));
	value.append("\"> </header> \n<data>\n");
	value.append(data);
	value.append("\n</data>\n</rdt-segment>\n");
	return value;
}

int readPacket(string *data){
	int count = 0;
	char c;
	*data = "";

	while((count < PACK_DATA_SIZE) && ((c = getchar()) != EOF)){
		*data += c;
		count++;
	}

	if(c == EOF)
		return 1;
	return 0;

}

int main(int argc, char **argv){
	in_port_t source_port = 0, dest_port = 0;
	in_addr_t dest_address = 0x7f000001;		//localhost
	int sel;

	vector<int> timers;
    vector<int> acks;

	if (argc != 5) {
		cerr << "Zadan chybny pocet parametru!" << endl << endl;
		usage();
		return EXIT_FAILURE;
	}

	for(int i = 1; i < 5; i += 2){
		if (!strcmp(argv[i],"-s")){
			source_port = atol(argv[i + 1]);
		}
		else if (!strcmp(argv[i], "-d")){
			dest_port = atol(argv[i + 1]);
		}
		else{
			cerr << "Zadan chybny parametr " << argv[i] << "!" << endl << endl;
			usage();
			return EXIT_FAILURE;
		}
	}

	if(!dest_port){
		cerr << "Nezadan parametr dest port!" << endl;
		usage();
		return EXIT_FAILURE;
	}
	if(!source_port){
		cerr << "Nezadan parametr source port!" << endl;
		usage();
		return EXIT_FAILURE;
	}

	//tato fce vraci handler pro udt connection
	int udt = udt_init(source_port);

	//nastaveni neblokujiciho cteni ze stdin
	fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

	//sdruzeni stdin a zdrojoveho portu do sady deskriptoru
	fd_set udt_stdin;
	FD_ZERO(&udt_stdin);
	FD_SET(udt, &udt_stdin);
	FD_SET(STDIN_FILENO, &udt_stdin);

	//casovac pro maximalni delku cekani na vstup
	timeval tv;
	tv.tv_sec = 100;
	tv.tv_usec = 0;

    //promenne potrebne uvnitr while cyklu
    int packet_id = 0, acked, inputFinished = 0;
    string packet = "";
    char c;
    int end = 0;
    int window_size = 1;
    timeval timer;
    int tack;

	while((sel = select(udt + 1, &udt_stdin, NULL, NULL, &tv)) && !end){
		if(sel == -1){
			cerr << "Nastala chyba pri volani select!";
			return EXIT_FAILURE;
		}

		//posilani paketu
		if(!inputFinished){
			for(int i = 0; i < window_size; i++){
				inputFinished = readPacket(&packet);
				gettimeofday(&timer, NULL);
				tack = timer.tv_usec / 1000.0;
				packet = createXML(packet_id, packet, window_size, tack);
				cout << packet;
				if(!udt_send(udt, dest_address, dest_port, (void *)packet.c_str(), packet.size())){
					cerr << "Nastala chyba pri volani udt_send!" << endl;
					return EXIT_FAILURE;
				}
				timers.push_back(0);
				acks.push_back(0);
				timers.at(packet_id) = tack;
				cout << "Tack: " << timers[packet_id];
				packet_id++;
				if(inputFinished)
					break;
			}
		}

		//prijimani acks
		if(udt_recv(udt, &packet, 500, NULL, NULL)){
			acked = getPackId(packet);
			for(int i = 0; i <= acked; i++){
				acks.at(i) = 1;
			}
		}

		//prisly nam vsechny acks
		if(acks.at(acks.size() - 1) == 1){
			cout << "Was in..." << endl;
			end = 1;
			for(int i = 0; i < acks.size(); i++){
				if(acks.at(i) != 1)
					end = 0; 
			}
		}

	}

	return 0;
}