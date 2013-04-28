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

string createXMLPacket(int sn, string data, int window_size, int time){
	string value = "<rdt-segment id=\"xskriv01\">\n<header sn=\"";
	value.append(convertInt(sn));
	value.append( "\" ack=\"potvrzeni\" win=\"");
	value.append(convertInt(window_size));
	value.append("\" tack=\"");
	value.append(convertInt(time));
	value.append("\"> </header> \n<data>");
	value.append(data);
	value.append("\n</data>\n</rdt-segment>");
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

	vector<string> XMLpackets;
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
    int packet_id = 0, acked, to_send = 0;
    char tempstr[500];
    string packet = "";
    int end = 0, lastSend = 0, acking = 0;
    int window_size = 1;
    timeval timer;
    int tack;
    int timeout1 = 10000, timeout2 = 1000, timeout3 = 1000, timeout;

	while((sel = select(udt + 1, &udt_stdin, NULL, NULL, &tv)) && !end){
		if(sel < 0){
			cerr << "Nastala chyba pri volani select!";
			return EXIT_FAILURE;
		}

		//posilani paketu
		if(!lastSend && !acking){
			for(int i = to_send; i < to_send + window_size; i++){
				//posledni posilani se zdarilo
				if(packet_id == i){
					lastSend = readPacket(&packet);
					gettimeofday(&timer, NULL);
					tack = timer.tv_usec / 1000;
					timeout = (tack + ((2*timeout1 + timeout2 + timeout3) / 4) )/2;
					timeout *= 1.3;
					packet = createXMLPacket(packet_id, packet, window_size, timeout);
					XMLpackets.push_back(packet);
					timers.push_back(timeout);
					acks.push_back(0);
					packet_id++;
				}
				//cout << "Pack id: " << packet_id << " to_send: " << to_send  << " i:" << i << endl;
				cout << "ze struktury: " << endl << XMLpackets.at(i);
				if(!udt_send(udt, dest_address, dest_port, (void *)XMLpackets.at(i).c_str(), XMLpackets.at(i).size())){
					//cerr << "Nastala chyba pri volani udt_send!" << endl;
					//return EXIT_FAILURE;
				}
				acking = 1;
				timers.at(i) = tack;
				if(lastSend)
					break;
			}
		}

		//prijimani acks
		if(udt_recv(udt, tempstr, 500, &dest_address, &dest_port)){
			packet.append(tempstr);
			gettimeofday(&timer, NULL);
			tack = timer.tv_usec / 1000;
			timeout3 = timeout2;
			timeout2 = timeout1;
			timeout1 = tack - getTack(packet);
			acked = getPackId(packet);
			cout << "Got acked: " << acked << endl;	
			acks.at(acked) = 1;
			acked++;
			to_send = acked;
			window_size++;
			acking = 0;
		}

		//vyprsel casovac
		gettimeofday(&timer, NULL);
		tack = timer.tv_usec / 1000;
		if( acking && (acks.at(packet_id - 1) == 0) && (tack > timers.at(packet_id - 1)) ){
			cout << "Timeout" << endl;
			window_size--;
			lastSend = 0;
			acking = 0;
		}

		//prisly nam vsechny acks - pokud je potvrzen posledni, potvrzuje tim i vsechny predchazejici
		if(lastSend && (acks.at(acks.size() - 1) == 1))
			end = 1;

	}

	return 0;
}
