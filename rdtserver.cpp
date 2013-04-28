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
#include <map>

//soubor obsahujici funkce pro vyuziti udt prenosu
#include "udt.h"

//konstanty pro nastaveni vlastnosti programu
#define PACK_DATA_SIZE 300
#define RTT 1000

using namespace std;

void usage(){
		cerr << "Program rdtserver" << endl;
		cerr << "Barbora Skrivankova, xskriv01@stud.fit.vutbr.cz" << endl;
		cerr << "Pouziti: ./rdtserver -s source_port -d dest_port" << endl;
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

string convertInt(int number)
{
   stringstream ss;
   ss << number;
   return ss.str();
}

string getPackData(string packet){
	unsigned pos, endpos;
	string ret;

	pos = packet.find("<data>");
	pos += 6;
	endpos = packet.find("</data>");
	if((pos != string::npos) && (endpos != string::npos)){
		ret = packet.substr(pos, endpos - pos - 1);
	}

	return ret;
}

string createXMLAck(int sn){
	string ret = "<rdt-segment id=\"xskriv01\">\n<header sn=\"";
	ret.append(convertInt(sn));
	ret.append( "\" ack=\"");
	ret.append(convertInt(sn));
	ret.append("\" win=\"win_size\" tack=\"timer\"> </header> \n<data>\n</data>\n</rdt-segment>");

	return ret;
}

int main(int argc, char **argv){
	in_port_t source_port = 0, dest_port = 0;
	in_addr_t dest_address = 0x7f000001;		//localhost
	int sel;

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

	char tempstr[500];
	string packet;
	int wantedPacket = 0;
	map<int, string> buffer;
	map<int, string>::iterator it;

	while((sel = select(udt + 1, &udt_stdin, NULL, NULL, NULL))){
		if(sel < 0){
			cerr << "Nastala chyba pri volani select!";
			return EXIT_FAILURE;
		}

		it = buffer.find(wantedPacket);
		if(it != buffer.end()){
			cout << it->second;
			buffer.erase(it);
			wantedPacket++;
			continue;
		}

		if(FD_ISSET(udt, &udt_stdin)){
			if(udt_recv(udt, tempstr, 500, &dest_address, &dest_port)){
				//cout << "Received: " << endl << tempstr << endl;
				packet.append(tempstr);
				memset(tempstr, 0, 500);
				int id = getPackId(packet);  
				string data = getPackData(packet);
				if(wantedPacket != id){
					buffer.insert(pair<int, string>(id, data));
				}
				else{
					cout << data;
					wantedPacket++;
				}
				//send ack
				packet = createXMLAck(id);
				if(!udt_send(udt, dest_address, dest_port, (void *)packet.c_str(), packet.size())){
					cerr << "Nastala chyba pri volani udt_send!" << endl;
					return EXIT_FAILURE;
				}
				packet.clear();
			}

		}
	}

	return 0;
}