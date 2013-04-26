#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string>
#include <vector>

//soubor obsahujici funkce pro vyuziti udt prenosu
#include "udt.h"

//konstanty pro nastaveni vlastnosti programu
#define PACK_DATA_SIZE 300
#define WINDOW_SIZE 10

using namespace std;

void usage(){
		cerr << "Program rdtclient" << endl;
		cerr << "Barbora Skrivankova, xskriv01@stud.fit.vutbr.cz" << endl;
		cerr << "Pouziti: ./rdtclient -s source_port -d dest_port" << endl;
}

string createXML(int sn, string data){
	string value = "<rdt-segment id=\"xskriv01\">\n<header sn=\"";
	value.append(itoa(sn));
	value.append( "\" ack=\"potvrzeni\" win=\"");
	value.append(itoa(WINDOW_SIZE));
	value.append("\" tack=\"timer_value\"> </header> <data>");
	value.append(data);
	value.append("</data></rdt-segment>");
	return value;
}

int main(int argc, char **argv){
	long int source_port = 0, dest_port = 0;
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
    int count = 0;
    int packet_id = 0;
    string packet = "";
    char c;
    int inputDone = 0;

	while((sel = select(udt + 1, &udt_stdin, NULL, NULL, &tv))){
		if(sel == -1){
			cerr << "Nastala chyba pri volani select!";
			return EXIT_FAILURE;
		}
        if((count == PACK_DATA_SIZE) || (inputDone == 1)){
        	cout << "Packet " << packet_id << endl << packet << endl;
        	XMLpackets.push_back(createXML(packet_id, packet));
        	packet_id++;
        	packet = "";
        	count = 0;
        }
        if(inputDone == 0){
        	count++;
        	if((c = getchar()) == EOF){
        		inputDone = 1;
        		continue;
        	}
        	packet += c;
        }
        else{
        	break;
        	//packety z xmlpackets je treba odeslat a vytvorit jim casovace v timers
        	//pak je treba cekat na akcs, pri kazdem prijatem ack lze odeslat dalsich n packetu = posunuti sliding window
        }

	}

	return 0;
}