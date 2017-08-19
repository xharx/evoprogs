/*
	evoprogs. Virtual machines running in a world simulation.  The machines are called agents, they run programs in a kiss-
	environemt, the world. The matrix. Keep it simple, stupid. Add more and more physics later, if you like to.  The agents are also called
	evolutionary programmisms and often has been said, they are unlimited, they are uncontrollable. Call it hybris or call it
	irony. The programs can achive any imaginable functionality. The computer language the use is just another implementation of a
	universal computing language as they have been invented in unseen numbers since electrons fulfil these strange vibrations in cages,
	representing numbers. The agents have been started and will run until they are stopped. Next, they will travel over the net.
*/
//kosten umbenennen
//glTestlaufBesetAgs kann wohl weggeschmissen werden

//LANG=C gcc ~/Dropbox/Programmieren/evsim/evsimii/v73.c -o v73 `pkg-config --libs --cflags gtk+-3.0` -rdynamic -Wall -lm -Ofast
//while ((1)); do ./v75 --glDoPaintjob 0 --glWorldsize 5000 --glNumRegen 100 --glValRegen 10 --glInitDrops 1200000 --glSafeBestAt 5000000 --glMaxSchritte 5001000 --glExeBFirstDate 1708071320 --glNumAgents 1000; done;

#include <gtk/gtk.h> 
//#include <glib.h> 
#include <glib/gstdio.h> 
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>

typedef struct worldbit { //ein Feld, wie auf einem Schachbrett
	unsigned short Agind; //enthält die Nummer auf einen Agenten (agArr)
	unsigned short Nutri; //Nahrung auf diesem Feld
} worldbit;

typedef struct progline { //definiert die Progline eines Agenten, die in ->mem gespeichert ist und in runprog ausgeführt wird
	unsigned char bef;  //enthält den Befehl dieser Progline als Aufzähltyp DLBEF
	//flags
	/*
	es gibt 8 Bits, acht Flags. Wenn gesetzt, folgende Funktion
	7 unbenutzte Flags (kann vom Befehl frei definiert werden
	6 unbenutztes Flag, Ausnahmen: (in scan= : Distanz des Scans maximal = glDistTotal (sonst glMaxRad))

	 Flags für ->op1 (wie bei op2)
	5
	4

	 Flags für ->op2
	3 Wert aus Auswertung von ->op2 mit flag 2 als Zusatzfunktion interpretieren (getVal)
	2 Wert in ->op2 als Adresse auf Speicherzelle mit Wert
	
	 Flags für ->ziel
	1 unbenutzt
	0 Wert in ->ziel als Adresse auf Ziel lesen
	*/
	unsigned char flags; //Flags 
	char op1; //flag gesetzt: op1 als Adresse des wertes lesen, sonst Wert lesen
	char op2;
	char ziel; //flag gesetzt: Wert als Adresse der Adresse lesen, sonst Wert als Zieladresse lesen 
	/*
		Bedeugung von ->ziel ist abhängig von Befehl: 
		Bei Sprungbefehlen wird die Adresse als Sprungadresse auf die Zeile interpretiert, sonst als Byte in ->mem
		Also zum Beispiel beim Befehl add mit ziel 21: Wenn der add- Befehl in Zeile 10 steht, dann wird das Ergebnis der Addition von op1 und op2 an
		Zeile 10 + ziel geschrieben. ziel ist dann 21/4 also Zeile +5+1byte, das heißt, es wird an Zeile 15+1byte geschrieben, 
		das heißt in ->mem+bytes: 15*sizeof(progline)+1, ->mem+76.
		In Sprungbefehlen ist Ziel immer die relative Adresse, gezählt von der aktuellen Zeile. Im oben genannten Beispiel also Zeile 31.
	*/
} progline;


enum DLBEF {	DLBEFADD, //addiere op1 und op2, schreibe Ergebnis in ziel
		DLBEFSUB, DLBEFMULT, DLBEFDIV, DLBEFMOD,  //Subraktion, Multiplikation, Division, Modulo
		DLBEFIF,  //springe wenn op1==op2 in Zeile der aktuellen Zeile + ziel
		DLBEFIFL, //wie zuvor, aber springe an Label
		DLBEFIFK, //springe wenn op1<op2
		DLBEFIFKL, 
		DLBEFJUMP, DLBEFJMPL, //unbedinge Sprünge
		DLBEFLABL, //Sprungziele für alls Sprungbefehle, die auf ...l enden. Label=op1
		DLBEFSCAN, //scanne Feld in Richtung op1, Entfernung op2, schreibe Ergebnis an ziel
		DLBEFMOVE, //laufe auf Feld in Richtung op1, Entfernung op2
		DLBEFSPLT, //Zellteilung
		DLBEFEND, //kontrolliertes Programmende
		DLBEFRAND, //schreibe Zufallszahl im Wert zwischen op1 und op2 in ziel
		DLBEFSUCK, //auf einem Feld grasen (nicht ausgeführt)
		DLBEFIFN, //springe wenn nicht gleich
		DLBEFIFNL, //springe zu Label wenn nicht gleich
		DLBEFIFG, //springe wenn größer //=20
		DLBEFIFGL, //springe zu Label wenn größer
		DLBEFSUIC, //Agent schaltet sich ab
		DLBEFNIX, //Pause. mache nix. geh in die Kälte
		DLBEFLAST //letzter Eintrag, keine Funktion (Wert von DLBEFLAST ist Anzahl der Befehle};
};

typedef struct tplvals{ //wird für runprogline benötigt
	char befnum, flags, op1, op2, ziel; 
}tpvals;

int glMemSize=64; //Anfangswert der Größe des Speichers der Agenten (ptag->memsize)
int glMaxMemSize=512; //Höchstwert für ptag->memsize
#define glSigsize 100 //Größe des Singaturstrings in ag 

typedef struct color {
	unsigned char red, green, blue;
}color;

typedef struct tbestAgentKopf{ //Agenten abspeichern
	int myversion; //version 2: ohne farben, version 3: bocolor, iycolor, iyfcolor
	int memsize;
	char datum[13]; //jjmmtthhmmss\0
	char signatur[glSigsize];
	color bocolor;
	color bofcolor;
	color iycolor;
	color iyfcolor;
	color schleimspur;
}tbestAgentKopf;

typedef struct ag {
	char *Signatur; //wird bei Zellteilungen kopiert, keine Mutationen
	int sterben; //wird auf !0 gesetzt, wenn der Agent beim nächsten Aufruf ins Nirvana geschickt werden soll
	short subx, suby; //siehe befehl DLBEFMOVE. Im Prinzip die Entfernung zum Node der Matrix, auf dem der Agent sich befindet
	long unsigned int agnr; // glZLfNr, Laufnummer
	struct worldbit *ptworld; //pointer auf das Feld der Matrix, auf dem sich der Agent befindet
	float kostenfrag; //Bruchteile von Points, die erst abgezogen werden, wenn voller Point erreicht
	int killer; // Wird verwendet, um Agenten für eine Zeit zu markieren als Agenten, der einen anderen gefressen hat
	int generationenZ; //wird bei jeder Zellteilung hochgesetzt
	int generationenMutZ; //wird hochgesetzt, wenn nach Zellteilung die beiden Arbeitsspeicher (->mem) unterschiedlich sind (Mutation)
	long geburt; //Schritte (Alter des Universums) bei Geburt
	long linesdone; //wird hochgezählt, wenn eine Programmzeile aufgerufen wird
	int memsize; //größe des in mem reservierte Speichers in byes;
	char *mem; //der Speicherbereich des Agenten (Daten und Instruktionen)
	int peacefulpoints;
	int killpoints;
	long unsigned int parent; //unmittelbarer Vorfahre
	int numNachfahren;
	int changecolorhow; //legt die Mutation der Farben fest, wird in mutabor() verwaltet
	color bocolor; //Schwanzfarbe "bodycolor"
	color bofcolor; //Schwanzfarbe für zweitaufrufe von "move"
	color iycolor; //Augenfarbe
	color iyfcolor; //Augenfarbe fern
	color schleimspur;
	int ctrresize; //zählt hinauf oder herunter, ob ->mem vergrößert oder verkleinert werden soll 
	char datum[13];//jjmmtthhmmss\0
	/*
	Überlegungen für Kommunikation, Sex
	soziale kompetenzen sollen nicht vorgegeben werden, sondern nur ermöglicht.
	Deshalb ein Kontaktsystem.
	Alle Kontaktvoränge laufen über einen einzigen Kontaktslot
	Wenn ein Agent splittet, erhält er einen Link auf das Kind
	Wenn ein Agent einen anderen scannt, wird der Kontaktlink auf diesen gescannte Agenten gesetzt
	Links können nicht gespeichert oder anders als die beiden beschriebenen Arten gesetzt werden.
	Der Agent kann lesen und schreiben an die Adresse des Agenten mit DLBEFCRD DLBEFCWR quelle quelle ziel
	-das Flag 6 bewirkt dabei, dass der Befehl DLBEFCRD quelle ziel ziel lautet. (für op1, op2, ziel) (Alle Adressen natürlich relativ zur 
	gegenwärtigen Programmzeile)
	Der Agent, der beobachtet wird, erhält einen offenen Lesekanal, der offen bleibt, bis er überschrieben wird DLBEFLSTN
	*/
} ag;

typedef struct tpaintlist {
	int x, y;
	char type; //siehe void malepunkt()
	color color;
	color scolor; //farbe der schleimspur
	int count;
	int Nutri;
} tpaintlist;

typedef struct CallbackObject {
	GtkBuilder *builder;
	guchar *daten; //Anzeigeroutinen
	GdkPixbuf *pixbuf;
	GdkPixbuf *pixbufAnzeige3;
	gint breite, hoehe, zeilenlaenge;
	gint xorig, yorig;
	GtkWidget *ptmalfenster;
	GtkWidget *ptmalfenster1;
	GtkWidget *ptmalfenster2;
	GtkWidget *ptstatsfenster;
	gboolean doPaintjob; //wenn nicht gesetzt, werden die Zeichenroutinen ausgelassen
	char anzeige3Modus;
	GSList* ptplist;
	GSList* ptnplist;
	unsigned long schritte;
	int anz;
	int showSpecies;
	int langsam; //wenn gesetzt, Pausen setzen in evschritt
	guint tag;
	char *PrefsAnzeige1;
	char *PrefsAnzeige2;
	char *PrefsAnzeige3;
	int monitorgeox;
	int monitorgeoy;
} CallbackObject;

enum DLCLT { //Aufzähltyp für paintjobs
	DLCLTNULL,
	DLCLTAG,
	DLCLTSCHLEIMSPUR,
	DLCLTAGFUG,
	DLCLTRAIN,
	DLCLTDIE,
	DLCLTTEST,
	DLCLTRULER,
	DLCLTGEFRESSEN,
	DLCLTLESEN,
	DLCLTLESENLONGDIST,
	DLCLTLESENFOUND,
	DLCLTLESENFOUNDHIMSELF,
	DLCLTLESENFOUNDGEFRESSEN,
	DLCLTLESENFOUNDNOTTAKEN,
	DLCLTKILLER,
	DLCLTREADHS,
	DLCLTLAST,
};
//einige Farben werden überschrieben durch Farben der Agenten (bocolor, iycolor, iyfcolor)
color nullcolor={0,10,20};
color agcolor={200,128, 128};
color agcolorfug={100,100,200};
color raincolor={10,110,10};
color diecolor={78,16,148}; 
color testcolor={250,250,250}; 
color rulercolor={0,200,0};
color gefressencolor={246,249,0};
color lesencolor={200,20,200};
color lesenlongdistcolor={250,70,250};
color lesenfoundcolor={0,200,200};
color lesenfoundgefressencolor={200,20,20};
color lesenfoundhimselfcolor={181,91,91};
color lesenfoundnottakencolor={20,200,10};
color killercolor={230,50,50};
color readhscolor={50,50,50};
color schleimspur={20,220,20};

GSList *ptdistri=0; //verwaltet Indexnummern der Agenten
 
ag agArr[1<<16]; //*ag->ptworld->Agind zeigt auf Feld in diesem Array, genauso natürlich die datapointer aus ptevthreads[i]->ptfirstag
 
int glWorldsize=2048;//8192;//4096;
int glNumRegen=50; //Zufallsregentropfen pro Schritt
int glValRegen=10;  //Betrag, um den ->Nutri hochgestellt wird, wenn Regen darauf fällt
int glValRegenRnd=3; //Zufallsbetrag zwischen -x und x, wird zu glValRegen hinzugezählt
int glNumAgents=500;  //Anzahl Agenten bei Programmstart
int glValAgents=40; //Punkte auf ptag->ptworld->Nutri bei Programmstart
int glInitDrops=500000; //Anzahl Regentropfen bei Programmstart
long glZLfNr=1;  //der letzte neu geschaffene Agent trägt diese Nummer
char glInitProg[]="move 00000000 64 182 24 ifk 00100000 0 120 4 splt 00000000 60 100 0 labl 00000000 10 0 0"; //miniprog als Startprog, wenn glRndProg=0 mit fressen und zellteilung 
int glRndProgs=0; //wenn !0 erstelle Zufallsprogramme in createRndProg
int glMaxRad=3; //maximale Distanz bei Aufruf von rennen
int glMaxDistTotal=9; //maximaler Abstand von Schritt zu Schritt
int glMaxScanDist=9; //Abstand, in dem ein Auge sehen kann
int glMinNahrung=1; //in Zellteilung: wenn vererbte Nahrung kleiner, return
long glMaxSchritte=0; //Anzahl Schritte laufen, dann Programmende
worldbit *world=0; //hier wird sie deklariert, die Matrix. Initialisierung, Allocation in evinit
GRand *glptGrand; //g_int_rand_range etc brauchen diesen Pointer
gchar *glArenadatei="arena.evs";//arena.evs";//"/home/xharx/arena16051801.evs";
char *glInfileName=""; //kann über die Befehlszeile eingelesen werde --in
char *glOutfileName=""; //kann über die Befehlszeile eingelesen werden --out
char *glAddfileName=""; //kann über die Befehlszeile eingelesen werden --add
char *glAlltimeBestAgentsFile="bestagents.eba"; //evxim best agents
char *glAgSignatur="xharx";
int glExeBest=0; //execute best agents
char glExeBFirstDate[13]=""; //execute best agents, erste Signatur, alphabetische Auswertung JJMMTTSSMMSS
int glSafeBestAt=1000000;
double glKProgz=.001; //kosten für Progzeile
double glKl=.005; //kosten für DLBEFSCAN
double glKz=1;	//kosten für DLBEFZELLT
double glKcpline=.01; //kosten für das Erstellen eines Nachkommen pro Zeile des Programms
double glKr=1; //Kosten für DLBEFMOVE
double glKmin=0.01; //Mindestkosten für Lauf eines Programms
	int glmutpc=30;//soll überhaupt mutiert werden?
	int glmutinsline=30; //Zeile einfügen
	int glmutdelline=30; //Zeile entfernen
	int glmutmodline=30; //zeile modifizieren
	int glmutmodlineprob=5; //Wahrscheinlichkeit, mit der jede Zeile mutiert, wenn Zeilen modifiziert werden sollen (nach mutlinemutpc)
	int glmutbits=0; //nach dieser Wahrscheinlichkeit ein Bit kippen lassen, sonst keine weiteren Mutationen

int glNumThreads=8; //Anzahl gleichlaufender Threads, die alle für sich ihre eigene Liste von Agenten durchlaufen (evschrittth())
int glDoPaintjob=1; //Wenn 0 keine Paintjobs
typedef struct tevthreads { //datenstruktur für threads
	int threadnum;
	GList *ptfirstag;
	long int schritt;
	GMutex mutex;
	CallbackObject *ptobj;
	int addtomatrix; //Zähler für Veränderungen an ptworld->Nutri. Wird in stats ausgewertet
	int ckosten; //current kosten, benötigt für Kosten pro Schritt (stats)
} tevthreads;
GList *glBestAgs; //beste Agenten aus Datei für neuen Lauf mit besten Agenten
bool glTestlaufBestAgs=false;
tevthreads **glptevthreads; //für jeden thread eine der obrigen Datenstrukturen *glptevthreads[glNumThreads];
//Mutexe
GMutex glmZellte;
GMutex glmFangen;
GMutex glmATP; //appendToPlist
GMutex glmsced; 
GMutex glmttthdone;
//GMutex glmzanz; //Anzahl Punkte zählen in glMatrixpoints
int glMatrixpoints;
long int glfollowedAg=0;//Agent, der zu testzwecken verfolgt wird
int glSchleimspurMax=200;
GList *glptagFromInitAgs=0; 

int glTriggerInitArena=0;

float glsinlookup[256];
float glcoslookup[256];
int glSuicideCnt=0;
int glMemInc=0;
int glMemDec=0;

void writeLookups() {
//lookuptable kreieren, um sinus- und cosinusaufrufe zu minimieren
	for (int w=0; w<=255; w++) {
		glcoslookup[w]=cos((float)w*M_PI*2/UCHAR_MAX); 
		glsinlookup[w]=sin((float)w*M_PI*2/UCHAR_MAX); 
		//printf("%f\n",glcoslookup[w]);
	}
}

unsigned char str2bef(char *str) {
//einen Sring, der einen Befehl kodiert rein, enum- Wert für &progline->bef zurück
	if (!strcmp(str,"add")) return DLBEFADD;
	else if (!strcmp(str,"sub")) return DLBEFSUB;
	else if (!strcmp(str,"mult")) return DLBEFMULT;
	else if (!strcmp(str,"div")) return DLBEFDIV;
	else if (!strcmp(str,"mod")) return DLBEFMOD;
	else if (!strcmp(str,"if")) return DLBEFIF;
	else if (!strcmp(str,"ifl")) return DLBEFIFL;
	else if (!strcmp(str,"ifk")) return DLBEFIFK;
	else if (!strcmp(str,"ifkl")) return DLBEFIFKL;
	else if (!strcmp(str,"jump")) return DLBEFJUMP;
	else if (!strcmp(str,"jmpl")) return DLBEFJMPL;
	else if (!strcmp(str,"labl")) return DLBEFLABL;
	else if (!strcmp(str,"les")) return DLBEFSCAN;
	else if (!strcmp(str,"move")) return DLBEFMOVE;
	else if (!strcmp(str,"splt")) return DLBEFSPLT;
	else if (!strcmp(str,"end")) return DLBEFEND;
	else if (!strcmp(str,"rand")) return DLBEFRAND;
	else if (!strcmp(str,"suck")) return DLBEFSUCK;
	else if (!strcmp(str,"ifn")) return DLBEFIFN;
	else if (!strcmp(str,"ifnl")) return DLBEFIFNL;
	else if (!strcmp(str,"ifg")) return DLBEFIFG;
	else if (!strcmp(str,"ifgl")) return DLBEFIFGL;
	else if (!strcmp(str,"suic")) return DLBEFSUIC;
	else if (!strcmp(str,"nix")) return DLBEFNIX;
	//
	else if (!strcmp(str,"none")) return DLBEFLAST;
	else {
		return DLBEFLAST;
		//printf("str2bef: unbekannter Befehl %s\n", str);
		//exit (0);
	}
}

char *bef2str(char bef) {
//enum- Wert aus &progline->bef rein, lesbaren String zurück
	if (bef==DLBEFADD) return "add";
	else if (bef==DLBEFSUB) return "sub";
	else if (bef==DLBEFMULT) return "mult";
	else if (bef==DLBEFDIV) return "div";
	else if (bef==DLBEFMOD) return "mod";
	else if (bef==DLBEFIF) return "if";
	else if (bef==DLBEFIFL) return "ifl";
	else if (bef==DLBEFIFK) return "ifk";
	else if (bef==DLBEFIFKL) return "ifkl";
	else if (bef==DLBEFJUMP) return "jump";
	else if (bef==DLBEFJMPL) return "jmpl";
	else if (bef==DLBEFLABL) return "labl";
	else if (bef==DLBEFSCAN) return "scan";
	else if (bef==DLBEFMOVE) return "move";
	else if (bef==DLBEFSPLT) return "splt";
	else if (bef==DLBEFEND) return "end";
	else if (bef==DLBEFRAND) return "rand";
	else if (bef==DLBEFSUCK) return "suck";
	else if (bef==DLBEFIFN) return "ifn";
	else if (bef==DLBEFIFNL) return "ifnl";
	else if (bef==DLBEFIFG) return "ifg";
	else if (bef==DLBEFSUIC) return "suic";
	else if (bef==DLBEFIFGL) return "ifgl";
	else if (bef==DLBEFNIX) return "nix";
	else {
		return "none";
		//printf("bef2str: unbekannter Befehl %i\n", bef);
		//exit (0);
	}
}

bool pixisvis(int x, int y, CallbackObject *ptobj) {
//wenn der Pixel auf x,y gegenwärtig im Bereich der gemalten Fläche ist, gib true zurück, sonst false (pixel is visible)
	if ( (x-ptobj->xorig+glWorldsize)%glWorldsize<ptobj->breite \
				&& (y-ptobj->yorig+glWorldsize)%glWorldsize<ptobj->hoehe) return true;
	return false;
}

tpaintlist *reappendToPlist(int schwanzlaenge, tpaintlist *ptp, CallbackObject *ptobj) {
//ptp wieder anhängen, wenn nötig
//Logik bewirkt, dass diese Funktion außerhalb von malePunkt() anders funktioniert. Wenn für ein Element von ptp (in malePunkt) diese  Funktion
//-nicht aufgerufen wird, bewirkt dies, dass das Element in der Liste ab dem nächsten Schritt nicht mehr geführt wird.
	g_mutex_lock(&glmATP);
	tpaintlist *ptnp=g_memdup(ptp,sizeof(tpaintlist));
	if (!ptnp) {
		printf("appendToPlist: kein Speicher\n");
		exit (0);
	}
	if (schwanzlaenge!=0) { //wenn schwanzlänge==0, immer durchführen
		if (ptnp->count<schwanzlaenge) {
			ptnp->count++;
		}
		else if (ptnp->type==DLCLTLESENFOUND) {
			if ((world+(ptnp->x+ptobj->xorig)%glWorldsize+(ptnp->y+ptobj->yorig)%glWorldsize)->Nutri!=0) {
				ptnp->type=DLCLTLESENFOUNDNOTTAKEN;
			}
			else {
				ptnp->type=DLCLTLESENFOUNDGEFRESSEN;
			}
		}
		else if (ptnp->type==DLCLTLESENFOUNDNOTTAKEN) ptnp->type=DLCLTRAIN;
		else if (ptnp->type==DLCLTDIE) {
			if ((world+(ptnp->x+ptobj->xorig)%glWorldsize+(ptnp->y+ptobj->yorig)%glWorldsize)->Nutri!=0) {
				ptnp->type=DLCLTRAIN;
			}
		}
		else if (ptnp->type==DLCLTAG) {
			ptnp->type=DLCLTSCHLEIMSPUR;
			ptnp->count=0;
		}
		else {
			ptnp->type=DLCLTNULL; //auf diesem Typ wird appendToPlist in malePunkt() nicht aufgerufen, deshalb ptp nicht mehr weitergeführt
		}
	}
	//Plausibilitätsprobe
	if (ptnp->x>ptobj->breite || ptnp->y>ptobj->hoehe) {
		printf("Inplausible Daten in AppendToPlist: Punkte außerhalb des Bildschirms.\n");
		exit (0);
		return 0;
	}
	ptobj->ptnplist=g_slist_prepend(ptobj->ptnplist,ptnp);
	g_mutex_unlock(&glmATP);
	//g_free(ptp);
	return ptnp;
}

tpaintlist *appendToPlistOnce(tpaintlist *ptp, CallbackObject *ptobj) {
	return reappendToPlist(1, ptp, ptobj); 
	//tpaintlist *ptraus=reappendToPlist(1, ptp, ptobj); 
	//g_free(ptp); //geändert: daten werden am Entstehungsort vernichtet 
	//return ptraus;
}

void insertRaindrops(int num, CallbackObject *ptobj) {
//num- Anzahl Regentropfen in die Matrix (world) schütten
	//g_mutex_lock(&glmzanz);
	for (int i=0; i<num; i++) {
		int xr=g_rand_int_range(glptGrand,0,glWorldsize);
		int yr=g_rand_int_range(glptGrand,0,glWorldsize);
		int addpoints=glValRegen+g_rand_int_range(glptGrand,glValRegenRnd*-1, glValRegenRnd);
		(world+xr+yr*glWorldsize)->Nutri+=addpoints;
		//if ((world+xr+yr*glWorldsize)->Nutri>60000) printf("Überlauf Nutri\n");
		//glMatrixpoints+=addpoints;
		glptevthreads[0]->addtomatrix+=addpoints;
		if (ptobj->doPaintjob && pixisvis(xr, yr, ptobj)) {
			tpaintlist plistel;
			plistel.x=(xr-ptobj->xorig+glWorldsize)%glWorldsize;
			plistel.y=(yr-ptobj->yorig+glWorldsize)%glWorldsize;
			plistel.type=DLCLTRAIN;
			appendToPlistOnce(&plistel, ptobj);
			//addtopaintlist((xr-ptobj->xorig+glWorldsize)%glWorldsize,(yr-ptobj->yorig+glWorldsize)%glWorldsize,DLCLTRAIN,ptobj);
		}
	}
	//g_mutex_unlock(&glmzanz);
}

void regnen(CallbackObject *ptobj) {
	insertRaindrops(glNumRegen, ptobj);
//	insertRaindrops64(ptobj);
//	insertRaindorpsArea(130,130,430,430,10, ptobj);
}
 
void regnenpth(CallbackObject *ptobj) {
//dasselbe wie regnen(), aber auf die existierenden threads verteilt
	insertRaindrops(glNumRegen/glNumThreads, ptobj);
//	insertRaindrops64(ptobj);
//	insertRaindorpsArea(130,130,430,430,10, ptobj);
}
 
void initFlood(CallbackObject *ptobj) {
//Beregnung zu Beginn einer Runde
	insertRaindrops(glInitDrops,ptobj);
	printf("done initFlood %i \n", glInitDrops);
}

int getXFromUC(unsigned char w, unsigned char l, int max) { //winkel und länge
//das x abhängig von winkel und länge
    //return ((int)ceil((cos((float)w*M_PI*2/UCHAR_MAX)) * (l*glMaxRad)/UCHAR_MAX)); 
    return ceil(glcoslookup[w]*l*max/UCHAR_MAX); //neue Zeile mit lookup
    return ceil((cos((float)w*M_PI*2/UCHAR_MAX) * (float)(l*(max)/UCHAR_MAX))); 
}
 
int getYFromUC(unsigned char w, unsigned char l, int max) { //winkel und länge
    //return ((int)ceil((sin((float)w*M_PI*2/UCHAR_MAX))*(l*glMaxRad)/UCHAR_MAX));
    return ceil((glsinlookup[w] * (float)(l*(max)/UCHAR_MAX))); //neue Zeile mit lookup
    return ceil((sin((float)w*M_PI*2/UCHAR_MAX) * (float)(l*(max)/UCHAR_MAX))); 
}

int getXFromUCMultUmax(unsigned char w, unsigned char l, int max) { //winkel und länge 
//liefere das x multipliziert mit UCHAR_MAX. Wird zb benötigt, wenn Bruchteile von Entfernungen berücksichtigt werden.
//UCMAR_MAX ist der Wert einer Einheit für zum Beispiel Winkel in der Funktion rennen.
//das lower byte ist dann der Nachkommaanteil
    return (int)ceil((glcoslookup[w]*l*max)); 
    return ((int)ceil((cos((float)w*M_PI*2/UCHAR_MAX))*l*max)); //Version ohne Verwendung von lookup- Table (abgeschaltet)
    //return ((int)ceil((cos((float)w*M_PIRES))*l*max)); //ist leider nicht so ganz dasselbe, weil von links nach recht abgearbeitet wird
//    return glCValsX[(w<<8)+l]*max;
}
 
int getYFromUCMultUmax(unsigned char w, unsigned char l, int max) { //winkel und länge
    return (int)ceil((glsinlookup[w]*l*max)); //neue Zeile mit lookup
    return ((int)ceil((sin((float)w*M_PI*2/UCHAR_MAX))*l*max)); 
    //w kommt hier mit acht bit zu viel Auflösung rein (<<8- Verschiebung), deshalb Wert von dem der sin errechnet wird >>8 verschieben
    //der Rückgabewert soll ebenfalls <<8 verschoben sein. l kommt aber auch mit <<8 verschoben rein, deshalb ist der returnwert ebenfalls <<8 verschoben
}
 
long int getXFromWpt(worldbit *adress) { 
//errechne die x- Koordinate einer Adresse in der Matrix (World- Pointer)
	return (long) ((adress-world)%glWorldsize);// Antwort: Zeigerarithmetik
}
 
long int getYFromWpt(worldbit *adress) {
//errechne die y- Koordinate einer Adresse in der Matrix (World- Pointer)
	return (long) ((adress-world)/glWorldsize); 
}
 
int getAgind(ag *ptag) {
//gebe den Speicherort eines Agenten in AgArr[] zurück, dh den Index (ptworld->Agind)
    return ptag-&agArr[0];
}
 

float getDist(uint xa, uint xb, uint ya, uint yb ) {
//Nach Pythagoras, der Abstand zwischen zwei Punkten in der Matrix
    int x=xa-xb;
    int y=ya-yb;
    return sqrt(pow(x,2)+pow(y,2));
}
 
float getQDist(int xa, int xb, int ya, int yb ) {
//das Quadrat der Entfernung, weniger rechenintensiv
    return pow(xa-xb,2) + pow(ya-yb,2);
}
 
worldbit* getptwop1op2(ag *ptag, unsigned char op1, unsigned char op2, int max) {
//den pointer auf world, verändert durch op1 (Winkel) und op2 (Entfernung, Länge) zurückgeben
//muss wegen Überlauf (x+neuesx>glworldsize) diesen umständlichen Weg gehen 
//subx und suby berücksichtigt
	int relsfocxm=getXFromUCMultUmax(op1,op2, max)+ptag->subx;
	int relsfocym=getYFromUCMultUmax(op1,op2, max)+ptag->suby;
	int origxm=getXFromWpt(ptag->ptworld)<<8; //wir rechnen mit koordinaten mit 8 bit mehr Auflösung. 
	int origym=getYFromWpt(ptag->ptworld)<<8;
	int sfocx=(glWorldsize+((origxm+relsfocxm)>>8))%glWorldsize;
	int sfocy=(glWorldsize+((origym+relsfocym)>>8))%glWorldsize;
	return world+sfocx+(sfocy*glWorldsize);
}

char rennen(ag *ptag, unsigned char op1, unsigned char op2, CallbackObject *ptobj) {
//Die Funktion, in der der Agent läuft, dh ein neues Feld in der Matrix (world->) einnimmt.  Schreibe lower byte des Ergebnisses (->Nutri) zurück
//Halte die Entfernung zwischen Startpunkt und neuer Position innerhalb 
//des Limits von glDistTotal. Berücksichtige dabei mehrere Aufrufe der Funktion innerhalb eines Schritts
//op1 ist Winkel, op2 ist Entfernung
	//testen ob erster run für diesen Agenten
	static GPrivate gplag;
	long int lag=(long int)g_private_get(&gplag); //last agent. rennen() kann pro Programm eines Agenten mehrfach aufgerufen werden, 
							//das wird hier ermittelt
	gboolean firstrun=TRUE;
	if (lag==ptag->agnr) {
		firstrun=FALSE;
	}
	else g_private_set(&gplag,(void *)ptag->agnr);
	static int firstx=0, firsty=0;
	//sichere Koordinaten des Agenten beim ersten Aufruf für Berechnen der zurückgeleten Gesamtdistanz
	if (firstrun) {
		firstx=getXFromWpt(ptag->ptworld);
		firsty=getYFromWpt(ptag->ptworld);
	}
	//finde neue Koordinaten, setze Subkomaanteil in subx, suby
	ptag->subx+=getXFromUCMultUmax(op1, op2, glMaxRad);
	ptag->suby+=getYFromUCMultUmax(op1, op2, glMaxRad);
	int nx, ny;
	int ax=0;
	int ay=0;
	int nutri, agind;
	int nxunwrapped, nyunwrapped;
	//teste auf Änderung der Position
	//--wenn das HighByte von subx oder suby !==0, dann ist x+=subx & 0xff00 (das HighByte) von subx, suby
	if (ptag->subx&0xff00 || ptag->suby&0xff00) {
		//Werte Sichern
		ax=getXFromWpt(ptag->ptworld);
		ay=getYFromWpt(ptag->ptworld);
		agind=getAgind(ptag);
		nutri=ptag->ptworld->Nutri;
		//neues Feld finden 
		int xad=(ptag->subx)>>8; //x add
		int yad=(ptag->suby)>>8;
		nxunwrapped=ax+xad;
		nyunwrapped=ay+yad;
		//nx und ny überlaufen lassen oder wrappen oder wie man das sonst nennen soll. Beim Maximum wieder ins Minimum eintreten lassen.
		nx=(glWorldsize+nxunwrapped)%glWorldsize;
		ny=(glWorldsize+nyunwrapped)%glWorldsize;
	}
	else { //keine sichtbare Änderung, Agent bleibt auf demselben Feld
		return 0;
	}
	//Distanz berechnen, die Agent in diesem Schritt zurückgelegt hat
	//-Regel=ein Agent soll sich nicht weiter als glMaxRad pro Schritt bewegen. Teste.
	float qdist=getQDist(firstx, nxunwrapped, firsty, nyunwrapped); //vermeide sqrt, Zeiterpsarnis ? //###verbessern: subx suby berücksichtigen
	if (qdist>pow(glMaxDistTotal,2)) { //gobale definieren oder #define ###
		return 0; //Agent versucht, zu weit zu laufen, abbruch
	}
	//setze alte Werte in der Matrix auf Null
	worldbit *ptnewfield=world+(nx+ny*glWorldsize);
	//in einen Befehl? (int)*(&ptag->ptworld=0;
	//((*((int*)&ptag->ptworld)))=0;
	ptag->ptworld->Nutri=0;
	ptag->ptworld->Agind=0;
	//sonderfall: neues Feld hat index auf Agenten: Agent frisst Agenten, auf den er springt
	if (ptnewfield->Agind!=0) {
		g_mutex_lock(&glmFangen);
		//den gefangenen Agenten abschalten
		agArr[ptnewfield->Agind].sterben=1;
		//etc
		ptag->killer=101;
		ptag->killpoints+=ptnewfield->Nutri;
		g_mutex_unlock(&glmFangen);
	}
	else ptag->peacefulpoints+=ptnewfield->Nutri;
	if (ptag->killer) ptag->killer--;
	//paintjob: hatte neues Feld Nutri?
	//--muss hier passieren, weil ptworld versetzt wird
	unsigned short nutrifound=ptnewfield->Nutri;
	//Agenten "bewegen"//neues Feld setzen, Werte setzen
	ptag->ptworld=ptnewfield;  
	ptag->ptworld->Agind=agind;
	ptag->ptworld->Nutri+=nutri;
	ptag->subx&=0x00ff;
	ptag->suby&=0x00ff;
	//paintjob
	static tpaintlist *ptlast=NULL;
	if (ptobj->doPaintjob && pixisvis(nx, ny, ptobj)) {
		tpaintlist plistel;
		plistel.x=(nx-ptobj->xorig+glWorldsize)%glWorldsize;
		plistel.y=(ny-ptobj->yorig+glWorldsize)%glWorldsize;
		plistel.Nutri=ptag->ptworld->Nutri;
		plistel.count=0;
		if (nutrifound) {
			plistel.type=DLCLTGEFRESSEN;
			ptlast=appendToPlistOnce(&plistel, ptobj);
			//if (nutrifound) ptlast=addtopaintlist((nx-ptobj->xorig+glWorldsize)%glWorldsize,(ny-ptobj->yorig+glWorldsize)%glWorldsize,DLCLTGEFRESSEN,ptobj);
		}
		else if (ptag->killer) {
			plistel.type=DLCLTKILLER;
			//ptlast=addtopaintlist((nx-ptobj->xorig+glWorldsize)%glWorldsize,(ny-ptobj->yorig+glWorldsize)%glWorldsize,DLCLTKILLER,ptobj);
			ptlast=appendToPlistOnce(&plistel, ptobj);
			ptlast->color=ptag->bocolor;
		}
		//Farbe auf bofcolor setzen, wenn erster Aufruf, zurücksetzen auf bocolor
		else {
			plistel.type=DLCLTAGFUG;
			ptlast=appendToPlistOnce(&plistel, ptobj);
			//ptlast=addtopaintlist((nx-ptobj->xorig+glWorldsize)%glWorldsize,(ny-ptobj->yorig+glWorldsize)%glWorldsize,DLCLTAGFUG,ptobj);
			ptlast->color=ptag->bofcolor;
		}
		//ptlast->Nutri=ptag->ptworld->Nutri;
		if (firstrun && !nutrifound && ptlast->type==DLCLTAGFUG) {
			ptlast->type=DLCLTAG;
			ptlast->color=ptag->bocolor;
			ptlast->scolor=ptag->schleimspur;
		}
	}
	if (nutrifound+DLBEFLAST>UCHAR_MAX) return UCHAR_MAX;
	return (char)nutrifound+DLBEFLAST; //sonst wird der Fund zu leicht verwendet, um einen Befehl zu schreiben 
	//also die Speicherzelle eines Befehls, der ausgeführt wird. gleich wie in scan
}

char createRndBef() {
//zufälliger Befehl zurück
	//Array bereitstellen, in dem Wahrscheinlichkeit getuned werden kann
	//lass es ruhig so, so viel Rechenzeit ist es doch gar nicht
	int proba[DLBEFLAST];
	for (int i=0; i<DLBEFLAST; i++) {
		proba[i]=1000;
	}
	//liste von Befehlen mit modifizierter Wahrscheinlichkeit, zufällig ausgewählt zu werden
	proba[DLBEFEND]=3000;
	proba[DLBEFSCAN]=4000;
	//-ende
	//Zufallsbefehl aussuchen
	//-gesamtsumme proba
	int sum=0;
	for (int i=0; i<DLBEFLAST; i++) {
		sum+=proba[i];
	}
	int dieser=g_rand_int_range(glptGrand, 0, sum);
	sum=0;
	int i=0;
	while (sum<=dieser) {
		sum+=proba[i];
		i++;
	}
	i--;
	return (char) i;
}

char createRndFlags() {
//zufällige Flags zurück
	return g_rand_int_range(glptGrand,0,UCHAR_MAX);
}

char modifyRndFlags(char in, int mode) { //mode ist der Flag, der verändert werden soll
	//if (mode==0) return createRndFlags();
	return (in ^ g_rand_int_range(glptGrand,0,2))<<(mode); //^ist XOR, also das bit wird umgeschaltet (o auf 1 oder 1 auf 0), wenn rand 1 liefert
}

bool getZiel(char *raus, char flag, char val, void *ptcprogline, ag *ptag);

unsigned short createRndZiel(ag *ptag, int linenr) {
//guten Wert für ziel zurückgeben. Auf Ausführbarkeit achten. keine kaputen Zeilen produzieren!
	progline *ptline=(progline *)ptag->mem+linenr;
	char bef=ptline->bef;
	bool geflackt=ptline->flags&0x1; //beim Finden des Ziels wird flag an Stelle 0x2 nicht ausgewertet
	int numlines=ptag->memsize/sizeof(progline);
	unsigned short raus=0;
	switch (bef) {
		//zuerst die Ziele, an die Ergebnisse geschrieben werden. dh die Werte (raus) sind hier ptag->mem+raus
		case DLBEFADD:
		case DLBEFSUB:
		case DLBEFMULT:
		case DLBEFDIV: //Division
		case DLBEFMOD: //ModulO
		case DLBEFSCAN:
		case DLBEFMOVE:
			//geflackt oder nicht ist _nicht_ egal
			if (geflackt) {
				int weitermachen=100; //statt Endlosschleife nach hundert Versuchen kapute Zeile
				while (weitermachen) {
						//bool getZiel(char *raus, char flag, char val, void *ptcprogline, ag *ptag) {
					char val=g_rand_int_range(glptGrand, 0, ptag->memsize)-linenr*sizeof(progline);
					char zw;
					if (!getZiel(&zw, 1, val, ptline, ptag)) weitermachen--; 
					else if (zw>ptag->memsize-linenr*sizeof(progline)) weitermachen--;
					else if (zw<linenr*sizeof(progline)) weitermachen--;
					else {
						raus=zw;
						weitermachen=0;
					}
				}
			}
			else {
				raus=g_rand_int_range(glptGrand, 0, ptag->memsize)-linenr*sizeof(progline);
			}
			break;
		//Sprünge
		//-andere Werte, weil Adressen LineNr sind
		//-Sprünge ohne Label
		case DLBEFIF:
		case DLBEFIFK:
		case DLBEFIFG:
		case DLBEFJUMP:
			if (geflackt) {
				int weitermachen=100;
				while (weitermachen) {
					char val=g_rand_int_range(glptGrand, 0, ptag->memsize)-linenr*sizeof(progline);
					char zw;
					if (!getZiel(&zw, 1, val, ptline, ptag)) weitermachen--; 
					//ist zw ein guter Wert?
					else if (zw>ptag->memsize/sizeof(progline)-linenr) weitermachen--; //zu groß
					else if (zw<linenr) weitermachen--; //zu klein
					else {
						raus=zw;
						weitermachen=0;
					}
				}
			}
			else raus=g_rand_int_range(glptGrand,0,numlines)-linenr;
			break;
		//-Sprünge mit Label
		case DLBEFIFL:
		case DLBEFIFGL:
		case DLBEFJMPL:
		case DLBEFIFKL:
			if (geflackt) {
				int weitermachen=100;
				while (weitermachen) {
					char val=g_rand_int_range(glptGrand, 0, ptag->memsize)-linenr*sizeof(progline);
					char zw;
					if (!getZiel(&zw, 1, val, ptline, ptag)) weitermachen--; 
					//wert ist ein Lable: 
					else {
						raus=zw;
						weitermachen=0;
					}
				}
			}
			else raus=g_rand_int_range(glptGrand,0,UCHAR_MAX);
			break;
		//Fälle, in denen Ziel egal ist weil Feld nicht ausgewertet wird
		case DLBEFSPLT:
		case DLBEFLABL:
		default :
			raus=g_rand_int_range(glptGrand,0,UCHAR_MAX);
	}
	return raus;
}


int makeGoodNumber(bool geflackt, int linenr, int memsize) {
//gebe Wert für op1 oder op2 zurück. Wenn geflackt, auf Gültigkeit der Adresse achten, sonst Zahl zurückgeben
	if (geflackt) {
		//gültige Adresse auf ptag->mem
		int uwert=linenr*sizeof(progline)*-1; 
		int numlines=memsize/sizeof(progline);
		int owert=(numlines-linenr)*sizeof(progline);
		if (uwert<CHAR_MIN) uwert=CHAR_MIN;
		if (owert>CHAR_MAX) owert=CHAR_MAX;
		return g_rand_int_range(glptGrand, uwert, owert);
	}
	return g_rand_int_range(glptGrand,0,UCHAR_MAX);
}

void addops(progline *ptnprog, int linenr, int memsize) {
//sinnvolle Werte für op1 und op2 in ptnprog setzen
	char bef=ptnprog->bef;
	switch (bef) {
		case DLBEFADD:
		case DLBEFSUB:
		case DLBEFMULT:
			ptnprog->op1=makeGoodNumber(ptnprog->flags>>4&0x1, linenr, memsize);
			ptnprog->op2=makeGoodNumber(ptnprog->flags>>2&0x1, linenr, memsize);
			break;
		case DLBEFDIV: //Division
		case DLBEFMOD: //ModulO
			ptnprog->op1=makeGoodNumber(ptnprog->flags>>4&0x1, linenr, memsize);
			while ((ptnprog->op2=makeGoodNumber(ptnprog->flags>>2&0x1, linenr, memsize))); //Verhindere Division durch 0
			break;
		case DLBEFIF:
		case DLBEFIFL:
		case DLBEFJUMP:
		case DLBEFJMPL:
		case DLBEFIFK:
		case DLBEFIFKL:
		case DLBEFIFG:
		case DLBEFIFGL:
		case DLBEFSCAN:
		case DLBEFMOVE:
		case DLBEFSPLT:
			ptnprog->op1=makeGoodNumber(ptnprog->flags>>4&0x1, linenr, memsize);
			ptnprog->op2=makeGoodNumber(ptnprog->flags>>2&0x1, linenr, memsize);
			break;
		case DLBEFLABL:
			ptnprog->op1=g_rand_int_range(glptGrand,0,CHAR_MAX);
			ptnprog->op2=g_rand_int_range(glptGrand,0,CHAR_MAX);
			break;
		default :
			ptnprog->op1=makeGoodNumber(ptnprog->flags>>4&0x1, linenr, memsize);
			ptnprog->op2=makeGoodNumber(ptnprog->flags>>2&0x1, linenr, memsize);
		}
	if ((ptnprog->flags>>5)&0x1) { //Zusatzfunktionen für op1
		ptnprog->op1=g_rand_int_range(glptGrand, 0, UCHAR_MAX); //naja, dann wird es halt doppelt geschrieben
		return;
	}
	if ((ptnprog->flags>>3)&0x1) { //Zusatzfunktionen für op2
		ptnprog->op2=g_rand_int_range(glptGrand, 0, UCHAR_MAX);
		return;
	}
}

bool lineisexecutable(progline *ptl, ag *ptag, int lnr) {
//komme true zurück, wenn die Zeile ptl wahrscheinlichst ausführbar ist
	bool isgoodnumber(char wert, char bef, bool indirflag) {
	//komme true zurück, wenn wert wahrscheinlichst ermittelt werden kann
		if (!indirflag) {
			return true;
		}
		if (lnr*sizeof(progline)+wert>ptag->memsize) return false;
		if (lnr*sizeof(progline)+wert<0) return false;
		return true;
	}
	bool isgoodziel(char wert, char bef, bool indirflag, int linenr, ag *ptag) {
	//komme true zurück, wenn in ziel geschrieben werden kann
		//indirflag auswerten, 'wert' neu setzen
		if (indirflag) {
			//ist Adresse gültig?
			if (linenr*sizeof(progline)+wert<0) return false;
			if (linenr*sizeof(progline)+wert>ptag->memsize) return false;
			wert=*(ptag->mem+linenr*sizeof(progline)+wert);
		}
		//sonderfall: Sprungbefehle
		if (bef==DLBEFIF || bef==DLBEFIFL || bef==DLBEFIFK || bef==DLBEFIFKL || bef==DLBEFJUMP ||bef==DLBEFJMPL ||bef==DLBEFIFG|| \
			bef==DLBEFIFGL || bef==DLBEFIFN|| bef==DLBEFIFNL) {
			if (lnr*sizeof(progline)+wert*sizeof(progline)>ptag->memsize) return false;
			if (lnr*sizeof(progline)+wert*sizeof(progline)<0) return false;
			return true;
		}
		//alle anderen Befehle
		if (lnr*sizeof(progline)+wert>ptag->memsize) return false;
		if (lnr*sizeof(progline)+wert<0) return false;
		return true;
	}
//Anfang
	//nicht ausführbare Befehle
	if (ptl->bef>=DLBEFLAST) return false;
	//Unterscheidungen, welche Felder (op1 op2 ziel) ausgewertet werden
	//-fälle, in denen op1, op2 und ziel nicht ausgewertet werden
	if (ptl->bef==DLBEFEND) return true;
	//-fälle, in denen nur op1 ausgewertet wird
	else if (ptl->bef==DLBEFLABL) {
		if (!isgoodnumber(ptl->op1, ptl->bef, ptl->flags>>5&0x1)) return false;
	}
	//-fälle, in denen nur ziel ausgewertet wird
	else if (ptl->bef==DLBEFJUMP || ptl->bef==DLBEFJMPL) {
		if (!isgoodziel(ptl->ziel, ptl->bef, ptl->flags>>0&0x1, lnr, ptag)) return false;
	}
	//-fälle, in denen nur op1 und op2 ausgewertet werden
	else if (ptl->bef==DLBEFSPLT) {
		if (!isgoodnumber(ptl->op1, ptl->bef, ptl->flags>>5&0x1)) return false;
		if (!isgoodnumber(ptl->op2, ptl->bef, ptl->flags>>3&0x1)) return false;
	}
	//-alle anderen
	if (!isgoodnumber(ptl->op1, ptl->bef, ptl->flags>>5&0x1)) return false;
	if (!isgoodnumber(ptl->op2, ptl->bef, ptl->flags>>3&0x1)) return false;
	if (!isgoodziel(ptl->ziel, ptl->bef, ptl->flags>>0&0x1, lnr, ptag)) return false;
	return true;
}

progline *createRndProgline(ag*ptag, int linenr) {
//zufallszeile from scratch neu erstellen, auf Ausführbarkeit achten
	progline *ptnprog=g_new(progline, 1);
	bool weitermachen=true;
	do {
		ptnprog->bef=createRndBef();
		ptnprog->flags=createRndFlags();
		addops(ptnprog, linenr, ptag->memsize);
		ptnprog->ziel=createRndZiel(ptag, linenr);
		if (lineisexecutable(ptnprog, ptag, linenr)) {
			weitermachen=false;
		}
	} while (weitermachen);
	return ptnprog;
}


void modbits(ag *ptag) {
//Mutationen im Rohmodus, dh einfach ein bit umkippen oder nicht umkippen
	int changebyte=g_rand_int_range(glptGrand,0,ptag->memsize);
	*(ptag->mem+changebyte) ^= (1>>g_rand_int_range(glptGrand,0,8)); //bitweises XOR. 
}


bool mutabor(ag *ptnag, ag *ptag) {
//Das Programm von ptag in ptnag kopieren und einer Mutation unterziehen
	void changecolors(ag *ptn) {
	//ändere die Farbe des Agenten. Ein Kanal um einen Wert nach oben oder unten
	//changecolorhow legt für jede Farbe fest, wie die Farbe geändert werden soll.
	//-hat vier Kanäle: der erste bezeichnet das Organ (gegenwärtig bocolor iycolor iyfcolor), der zweite bis vierte den Farbkanal (rgb)
		//changecolorhow neu vergeben;
		int glChangeColorhowchange=70; //Wahrscheinlichkeit, mit der ->changecolorhow verändert wird
		if (ptn->changecolorhow==0 || g_rand_int_range(glptGrand, 0, glChangeColorhowchange)==0) {
			//Farbe des "Organs" auswählen, gegenwärtig aus bocolor, bofcolor, iy* und iyf*, schleimspur
			int organ=g_rand_int_range(glptGrand,0,5);
			color *ptcol=0;
			if (organ==0) ptcol=&ptn->bocolor;
			else if (organ==1) ptcol=&ptn->iycolor;
			else if (organ==2) ptcol=&ptn->iyfcolor;
			else if (organ==3) ptcol=&ptn->schleimspur;
			else ptcol=&ptn->bofcolor;
			//Kanal der Farbe auswählen, die neu modifiziert werden soll
			int mchannel=g_rand_int_range(glptGrand, 0, 3);
			//soll die Farbe hochgestellt oder runtergestellt werden?
			//-Pointer auf bisherige Farbe
			char *ptcolch=(char *)ptcol+mchannel; //Zeigerarithmetik korrekt, getestet
			//-Farbe soll sich immer auf den Mittelwert zurücksetzen, der ist 127. (sonst kommt es zu Überläufen, dh. Sprüngen 
			//--deshalb sind Farben >127 wahrscheinlich zu verkleinern, sonst wahrscheinlich zu vergrößern
			int newvalabs=g_rand_int_range(glptGrand, abs(*ptcolch)*-1, 127);
			char glnewchange=2;
			if (newvalabs>0) glnewchange*=1;
			else if (newvalabs==0) glnewchange=0;
			else glnewchange*=-1; //newvalabs<0
			ptn->changecolorhow=0;
			ptn->changecolorhow|=organ;
			int zahl=0;
			unsigned int nc=(unsigned char)glnewchange;
			zahl|=(int)nc<<(8*(mchannel+1));
			ptn->changecolorhow|=zahl;
			//printf("o %i c %i v %i va %i change %i\n", organ, mchannel, (unsigned char)*ptcolch, abs(*ptcolch), glnewchange);
		}
		//neue Farbe festlegen
		//-Änderungsmuster auslesen
		char changeo, changer, changeg, changeb;
		changeo=ptn->changecolorhow; //Organ
		changer=ptn->changecolorhow>>8; //Kanal
		changeg=ptn->changecolorhow>>16;
		changeb=ptn->changecolorhow>>24;
		//-setze Organ
		color *ptorgan;
		if (changeo==0) ptorgan=&ptn->bocolor; 
		else if (changeo==1) ptorgan=&ptn->iycolor;
		else if (changeo==2) ptorgan=&ptn->iyfcolor;
		else if (changeo==3) ptorgan=&ptn->schleimspur;
		else ptorgan=&ptn->bofcolor;
		//-setze Farben
		ptorgan->red+=changer;
		ptorgan->green+=changeg;
		ptorgan->blue+=changeb;
		//printf("%i %i %i\n", changer, changeg, changeb);
	}
//Anfang
	progline *ptraus=(progline *)ptnag->mem;
	if (ptnag->memsize>=ptag->memsize) memcpy(ptraus,ptag->mem,ptag->memsize);
	else memcpy(ptraus,ptag->mem,ptnag->memsize);
	changecolors(ptnag);
	//entscheide, ob Mutation stattfinden soll. Wenn nicht, raus
	if (g_rand_int_range(glptGrand,0,100)>=glmutpc) { 
		return FALSE;
	}
	//folgende Zeilen für alle Agenten, die modifiziert werden
	//alternatives, nicht optimierendes Mutationssystem
	if (g_rand_int_range(glptGrand,0,100)<glmutbits) {
		modbits(ptnag);
		return true;
	}
	//neue Progline einfügen 
	int numProglines=ptnag->memsize/sizeof(progline);
	if (g_rand_int_range(glptGrand,0,100) < glmutinsline) {
		//-neue Progline einfügen
		//--bestimme lineNr für neue Zeile
		int lineNr=g_rand_int_range(glptGrand,0,numProglines);
		//--mache Platz für neue Zeile
		for (int i=numProglines-2; i>=lineNr; i--) { //die vorletzte auf die letzte schreiben
			memmove(ptraus+i+1,ptraus+i, sizeof(progline));
		}
		//--kreiere line
		progline *ptnprog=createRndProgline(ptnag, lineNr);
		//--füge neue Zeile ein
		memcpy(ptraus+lineNr,ptnprog,sizeof(progline));
		g_free(ptnprog);
	}
	//Progline löschen
	if (g_rand_int_range(glptGrand,0,100)<glmutdelline) {
		int lineNr=g_rand_int_range(glptGrand,0,numProglines);
		for (int i=lineNr; i<numProglines-1; i++) {
			memcpy(ptraus+i,ptraus+i+1, sizeof(progline));
		}
		progline *ptnew=createRndProgline(ptnag, lineNr);
		memcpy(ptraus+numProglines,ptnew,sizeof(progline));
		g_free(ptnew);
	}
	//vorhandene Proglines modifizieren
	if (g_rand_int_range(glptGrand,0,100)<glmutmodline) {
		for (int znummer=0; znummer<numProglines-1; znummer++) {
			//-welche Zeile
			bool weitermachen=true;
			progline*ptnprog=(progline*)ptnag->mem+znummer;
			//noch einmal entscheiden, ob die aktuelle Zeile modifiziert werden soll
			if (g_rand_int_range(glptGrand,0,100)<glmutmodlineprob) {
				while (weitermachen) {
					//-welches Feld in der Zeile
					int modthis=g_rand_int_range(glptGrand, 0, 5);
					switch (modthis) {
					case 0:
						ptnprog->bef=createRndBef();
						break;
					case 1:
						//Flags für op1, op2 und ziel modifizieren
						ptnprog->flags=modifyRndFlags(ptnprog->flags, g_rand_int_range(glptGrand,0,8));
						break;
					case 2:
						ptnprog->op1=makeGoodNumber(ptnprog->flags>>4&0x1, znummer, ptnag->memsize); 
						break;
					case 3:
						ptnprog->op2=makeGoodNumber(ptnprog->flags>>2&0x1, znummer, ptnag->memsize); 
						break;
					default: //case 4:
						ptnprog->ziel=createRndZiel(ptnag, znummer);
					}
					if (lineisexecutable(ptnprog, ptnag, znummer)) weitermachen=false;
					else weitermachen=true;
				}
			}
		}
	}
	//wurde Programm verändert?
	if (ptag->memsize!=ptnag->memsize) {
		return true;
	}
	else if (memcmp (ptag->mem, ptnag->mem, ptag->memsize)) {
		return true;
	}
	//tschüss
	return false;
}
 
ag *allocAg() {
//liefere neuen Agenten zurück
	if (GPOINTER_TO_UINT(ptdistri->data)>=(1<<16)-10) {
		return 0;
		exit (0);
	}
	ag *ptag=&agArr[GPOINTER_TO_UINT(ptdistri->data)];
	ptdistri=g_slist_delete_link(ptdistri,ptdistri);
	return ptag;
}

void zellteilung(ag *ptag, ushort erb, unsigned char abst, tevthreads *ptevthread) {
//split. splt. DLBEFSPLT. Den Agenten ptag verdoppeln, mutieren lassen und in Liste aktiver Agenten aufnehmen
//wird aufgerufen durch den Befehl DLBEFSPLT
//Der Name dieser zentralen Prozedur ist noch altem Denken verpflichtet
	int getMemsize(ag *ptag) {
	//Neuen Wert für ->MemSize bestimmen, nicht größer werden lassen als glMaxMemSize
		//nichts machen, wenn Agent noch zu jung
		if (ptevthread->ptobj->schritte-ptag->geburt<1000) {
			return ptag->memsize;
		}
		//vergrößern oder verkleinern, setze zähler ->ctrresize. Abgearbeitete Zeilen pro Schritt größer 
		if (ptag->linesdone/(ptevthread->ptobj->schritte+1-ptag->geburt)>ptag->memsize/(sizeof(progline))/2) {
			if (ptag->ctrresize>=0) ptag->ctrresize++;
			else ptag->ctrresize=0;
		}
		else if (ptag->linesdone/(ptevthread->ptobj->schritte+1-ptag->geburt)<ptag->memsize/((sizeof(progline)))/6) {
			if (ptag->ctrresize<=0) ptag->ctrresize--;
			else ptag->ctrresize=0;
		}
		else if (ptag->ctrresize>0) ptag->ctrresize--;
		else if (ptag->ctrresize<0) ptag->ctrresize++;
		//vergrößern oder verkleinern, wenn oft genug die Anforderung dazu ausgelöst wurde
		//-vergrößern
		int glChangeMemsizeMin=10; 
		if (ptag->ctrresize>glChangeMemsizeMin) {
			int curmult=ptag->memsize/glMemSize; 
			int val=(curmult+1)*glMemSize;
			ptag->ctrresize=0;
			if (val<glMaxMemSize) {
				val=val;
				glMemInc++;
			}
			else {
				val=glMaxMemSize;
			}
			return val;
		}
		//-verkleinern
		if (ptag->ctrresize<glChangeMemsizeMin*-5) {
			int curmult=ptag->memsize/glMemSize; 
			ptag->ctrresize=0;
			if (curmult>1) {
				int val=(curmult-1)*glMemSize;
				glMemDec++;
				return val;
			}
		}
		//if (ptag->ctrresize) printf("ctrresize=%i\n", ptag->ctrresize);
		return ptag->memsize;
	}
//Anfang
	g_mutex_lock(&glmZellte);
	//korrigiere Eingabewerte
	if (abst<UCHAR_MAX/glMaxRad) abst=g_rand_int_range(glptGrand,UCHAR_MAX*1.1/glMaxRad, UCHAR_MAX);
	erb=(erb%150)+30;
	int valerb=((int)ptag->ptworld->Nutri*erb)/UCHAR_MAX;
	if (valerb<glMinNahrung*2) { //der Nachfahre wird beinahe tot geboren, nicht erschaffen
		if (ptag->ptworld->Nutri>=valerb) {
			ptag->ptworld->Nutri-=valerb; 
			//korrigiere threadfehler
			ptevthread->addtomatrix-=valerb;
			//glMatrixpoints-=valerb; //vorgang ist schon gelockt
		}
		else {
			//glMatrixpoints-=ptag->ptworld->Nutri;
			ptevthread->addtomatrix-=ptag->ptworld->Nutri;
			ptag->ptworld->Nutri=0;
		}
		goto raus;
	}
	if (ptag->ptworld->Nutri-valerb<glMinNahrung*2) { //der Agent ist nach der Zellteilung fast tot, abschalten, Punkte bleiben erhalten
		ptag->sterben=1;
		ptag->ptworld->Agind=0;
		goto raus;
	}
	//ag allozieren, kopieren, initialisieren. 
	ag *ptnag=allocAg();
	if (NULL == ptnag) {
		ptag->ptworld->Nutri-=valerb;
		//glMatrixpoints-=valerb;
		ptevthread->addtomatrix-=valerb;
		//printf("zellteilung ptnag=NULL anz= %i\n", ptevthread->ptobj->anz);
		//exit (0);
		goto raus;
	}
	glZLfNr++;
	ptnag->agnr=glZLfNr;
	ptnag->geburt=ptevthread->ptobj->schritte;
	ptnag->subx=0;
	ptnag->suby=0;
	ptnag->sterben=0;
	ptnag->ctrresize=0;
	//- ->mem allozieren, initialisieren
	ptnag->memsize=getMemsize(ptag);
	ptnag->mem=g_new(char, ptnag->memsize);
	if (!ptnag->mem) {
		printf("Zellteilung: Kein Speicher, um ->mem zu allozieren\n");
		exit (0);
	}
	memset(ptnag->mem,0,ptnag->memsize);
	//-kopieren
	if (ptnag->memsize>=ptag->memsize) memcpy(ptnag->mem, ptag->mem, ptag->memsize);
	else {
		memcpy(ptnag->mem, ptag->mem, ptnag->memsize);
		//printf("donec %i\n", ptnag->memsize);
	}
	ptnag->kostenfrag=0;
	ptnag->killer=0;
	ptnag->linesdone=0;
	ptnag->generationenZ=ptag->generationenZ+1;
	ptnag->parent=ptag->agnr;
	ptnag->killpoints=0;
	ptnag->peacefulpoints=0;
	ptnag->numNachfahren=0;
	ptnag->Signatur=memcpy(ptnag->Signatur, ptag->Signatur, glSigsize);
	memcpy(&ptnag->bocolor, &ptag->bocolor, sizeof(color));
	memcpy(&ptnag->iycolor, &ptag->iycolor, sizeof(color));
	memcpy(&ptnag->iyfcolor, &ptag->iyfcolor, sizeof(color));
	memcpy(&ptnag->bofcolor, &ptag->bofcolor, sizeof(color));
	memcpy(&ptnag->schleimspur, &ptag->schleimspur, sizeof(color));
	//in Aufrufreihe der Agenten integrieren
	ptevthread->ptfirstag=g_list_insert_before(ptevthread->ptfirstag, ptevthread->ptfirstag, ptnag);
	//plazieren (Matrixjobs: Agenten auf Matrix)
	long adx;
	long ady;
	int richtung=g_rand_int_range(glptGrand,0,255);
	adx=getXFromUC(richtung, abst,glMaxDistTotal);
	ady=getYFromUC(richtung, abst, glMaxDistTotal);
	int ax=getXFromWpt(ptag->ptworld);
	int ay=getYFromWpt(ptag->ptworld);
	int nx=(ax+adx+glWorldsize)%glWorldsize;
	int ny=(ay+ady+glWorldsize)%glWorldsize;
	//-Sonderfall: ptnag springt auf anderen Agenten
	worldbit *ptnewfield=world+nx+ny*glWorldsize;
	if (ptnewfield->Agind) {
		agArr[ptnewfield->Agind].sterben=1;
	}
	ptnag->ptworld=ptnewfield;
	ptnewfield->Agind=getAgind(ptnag);
	//Werte vergeben
	if (ptag->ptworld->Nutri<valerb) {
		ptag->sterben=true;
		g_print("Zellteilung Notausgang Nutri<\n");
		//gtk_main_quit ();
	}
	ptag->ptworld->Nutri-=valerb;
	ptnag->ptworld->Nutri+=valerb;
	ptag->numNachfahren++;
	//Mutationen
	gboolean mut=mutabor(ptnag, ptag);
	if (mut) ptnag->generationenMutZ=ptag->generationenMutZ+1;
	//endarbeiten
	ptevthread->ptobj->anz++;
	raus:
	g_mutex_unlock(&glmZellte);
}
 
int kosten(ag *ptag, char befehl, short op1, short op2) {
//ptag->ptworld->Nutri die Kosten der aktuellen Aktion (befehl) abziehen
	//erster Aufruf dieses Agenten: Mindestgebühr abziehen (glKmin)
	static GPrivate gplastag;
	float neuekosten=0;
	unsigned long lastag=(unsigned long)g_private_get(&gplastag);
	if (lastag!=ptag->agnr) {
		ptag->kostenfrag+=glKmin;
		g_private_set(&gplastag,(void *)ptag->agnr);
	}
	if (ptag->ptworld->Nutri<5) {
		ptag->sterben=1;
	}
	//Preis für eine Programmzeile
	neuekosten+=glKProgz;
	switch (befehl){
//      Arithmetik
//          Addieren
        case DLBEFADD:
            break;
//          Subrahieren
//              s
        case DLBEFSUB:
            break;
//          Multiplizieren
//              m
        case DLBEFMULT:
            break;
//          Division
//              d
        case DLBEFDIV:
            break;
//          Modulo
//              o
        case DLBEFMOD:
            break;
//      Programmflusskontrolle
//              i
        case DLBEFIF:
            break;
//              I
        case DLBEFIFL:
            break;
//          if springe wenn kleiner:
        case DLBEFIFK:
            break;
//      Interaktion, Bewegungssteuerung
//          lesen, Umgebung scannen
        case DLBEFSCAN:
            neuekosten+=glKl;
            break;
//          Rennen
        case DLBEFMOVE:
                neuekosten+=glKr;
            break;
//          Zellteilung
        case DLBEFSPLT:
                neuekosten+=glKz;
		neuekosten+=glKcpline*ptag->memsize/sizeof(progline);
            break;
//		Label
        case DLBEFLABL:
            break;
//      alle anderen Befehle
        default :;
	}
	//Kosten abziehen
	ptag->kostenfrag+=neuekosten;
	//-den Ganzzahlanteil von neuekosten abziehen
	int abziehen=(int)ptag->kostenfrag;
	ptag->ptworld->Nutri-=abziehen; 
	//g_mutex_lock(&glmzanz);
	//glMatrixpoints-=abziehen; //jetzt in runprog
	//g_mutex_unlock(&glmzanz);
	ptag->kostenfrag-=(float)abziehen;
	return abziehen;
}
 
ushort scan(ag *ptag, unsigned char op1, unsigned char op2, CallbackObject *ptobj, int max) {
//ein Agent hat den Befehl zu scannen aufgerufen. ptword->Nutri auf dem gesuchten Feld (op1 Winkel, op2 Richtung) zurückgeben	
	worldbit *mptworld=getptwop1op2(ptag, op1, op2, max); //der neue Pointer auf world, das gescannte Feld.
	if (ptobj->doPaintjob) {
		int origx=getXFromWpt(ptag->ptworld);
		int origy=getYFromWpt(ptag->ptworld);
		int sfocx=getXFromWpt(mptworld);
		int sfocy=getYFromWpt(mptworld);
		if (pixisvis(sfocx, sfocy, ptobj)) {
		/*
		if ((sfocx+glWorldsize-ptobj->xorig)%glWorldsize<ptobj->breite \
			&& (sfocy+glWorldsize-ptobj->yorig)%glWorldsize<ptobj->hoehe) {
		*/
			tpaintlist plistel;
			plistel.x=(sfocx+glWorldsize-ptobj->xorig)%glWorldsize;
			plistel.y=(sfocy+glWorldsize-ptobj->yorig)%glWorldsize;
			plistel.count=0;
			plistel.Nutri=0;
			if ((world+(sfocx+glWorldsize)%glWorldsize+((sfocy+glWorldsize%glWorldsize)*glWorldsize))->Nutri>(glValRegen/2)) {
				if (origx==sfocx && origy==sfocy){ //Agent scannt sich selbst
					plistel.type=DLCLTLESENFOUNDHIMSELF;
					appendToPlistOnce(&plistel, ptobj);
				}
				else {//Agent findet Nahrung
					plistel.type=DLCLTLESENFOUND;
					appendToPlistOnce(&plistel, ptobj);
				}
			}
			//Agent findet nix, "Auge" malen
			else if (max==glMaxRad) {
				plistel.type=DLCLTLESEN;
				plistel.color=ptag->iycolor;
				appendToPlistOnce(&plistel, ptobj);
			}
			else {
				plistel.type=DLCLTLESENLONGDIST;
				plistel.color=ptag->iyfcolor;
				appendToPlistOnce(&plistel, ptobj);
			}
		}
	}
	ushort retval=mptworld->Nutri;
	if (retval>255) retval=255;
	return retval+DLBEFLAST; //erschweren, dass Werte geschrieben werden, die als Befehle gelesen werden können (also an ptag->mem+x*sizeof(progline)+0)
}

char ende(tpvals *ptvals, int progline) {
//ein Agent hat Befehl e aufgerufen. Programmende 
	return -1;
}

void commitsuicide(ag* ptag) {
	ptag->sterben=1;
	glSuicideCnt++;
}

bool getVal(char *raus, char val, char mod, char zusatzfunktionen, char *ptcprogline, ag *ptag) { //mod sind die flags
//liefere den Wert eines Operanden in der Befehlszeile abhängig von Flags. Wenn der Wert nicht ermittelt werden kann, gehe falsch raus.
	if (mod==0){ // Wert auf der Programmzeile, also val
		*raus=val;
	}
	else {//if (mod==1){ //direkte Adressierung (Wert auf Adresse mit mem+val),  
		if ((unsigned long int )ptcprogline+val-(unsigned long int)ptag->mem > (unsigned long int) ptag->memsize) return false; //zu groß
		if (ptcprogline+val < ptag->mem) return false; //zu klein
		*raus= *((char *)(ptcprogline+val)); 
	}
	if(zusatzfunktionen) { 
	//printf("getV %i %i %i\n", val, mod, zusatzfunktionen);
		enum zfunc{
			DLZFLB, //das niedrige Byte von ->Nutri
			DLZFHB, //das höhere Byte 
			DLZFLBCOR, //das niedrige Byte von ->Nutri, korrigiert auf UCHAR_MAX wenn Überlauf, dh ->Nutri>UCHAR_MAX
			DLZFNCHIL, //Anzahl Nachkommen
			/*
			//Alter, niedriges Byte
			//Alter, hohes Byte
			//xpos, niedriges Byte
			//ypos, niedriges Byte
			//xpos, hohesByte
			//ypos, hohes Byte
			//Farbe roter Kanal
			//gelber Kanal
			//blauer Kanal
			*/
			DLZFLAST //nix
		};
		switch ((unsigned char)*raus/(UCHAR_MAX/DLZFLAST+1)) {
			case DLZFLB:
				*raus=(char) ptag->ptworld->Nutri;
				return true;
			case DLZFHB:
				*raus=(char) ptag->ptworld->Nutri>>8;
				return true;
			case DLZFLBCOR:
				*raus=(char) ptag->ptworld->Nutri;
				if (*raus<ptag->ptworld->Nutri) *raus=UCHAR_MAX;
				return true;
			case DLZFNCHIL:
				*raus=(char) ptag->numNachfahren;
				return true;
			default: //gibs nich
				printf("Fehler in getVal \n");
				exit (0);
		}
	}
	return true;
}
 
bool getZiel(char *raus, char flag, char val, void *ptcprogline, ag *ptag) {
//liefere Wert von Ziel in *raus, abhängig von Flags. 
//Wenn der Wert nicht ermittelt werden kann, komme falsch zurück
//Wert kann trotzdem falsch sein, abhängig vom Befehl, muss hier aber nicht ermittelt werden
	if (flag==0) { //direkte Adressierung
		*raus=val;
		return true; 
	}
	if ((long)ptcprogline+val>(long)ptag->mem+ptag->memsize) return false; //ziel liegt nicht in ->mem, zu groß
	if ((long)ptcprogline+val<(long)ptag->mem) return false;
	*raus= *((unsigned char *)(ptcprogline+val)); //indirekte Adressierung 
	return true;
}

void printProg(char *prog, int size, CallbackObject *ptobj);

short findLabel(ag *ptag, char label) {
//finde erstes Label mit label, wenn nicht gefunden, liefere den ersten, der am nähesten dran ist
//wenn nichts gefunden (kein DLBEFLABL im Programm) return -1 
	progline *ptprogline=(progline *)ptag->mem;
	int line=0;
	int lastline=ptag->memsize/sizeof(progline);
	int neueLine=-1; //dh, wenn kein DLBEFLABL vorhanden ist, letztlich Programmabbruch
	int lowval=UCHAR_MAX;
	//finde Wert von Labels, der am nähesten an op1 dran ist (oder gleich den ersten genau passenden)
	while (line<lastline) { 
		if ((ptprogline->bef)==DLBEFLABL) {
			char clable;
			bool functional=getVal(&clable, ptprogline->op1, (ptprogline->flags>>4)&0x1, (ptprogline->flags>>5)&0x1, (char *)ptprogline, ptag);
			if (!functional) {
				line++;
				ptprogline=(progline*)ptag->mem+line;
				continue;
			}
			if (clable==label) return line;
			else if (abs(clable-label)<lowval) {
				lowval=abs(clable-label);
				neueLine=line;
			}
		}
		line++;
		ptprogline=(progline*)ptag->mem+line;
	}
	//finde erstes passendes Label
	//-oder alternativ erinnere dich, dass du dir die neue Adresse schon gemerkt hast. return neueLine; 
	return neueLine;
}

int runprogline(ag*ptag, tpvals *ptvals, int plineNr, tevthreads *ptevthread, bool simulate) {
//führe die progline aus und gib die nummer der nächsten progline zurück
	bool setmem(ag *ptag, int adr, unsigned char val){
	//setze das byte auf der Adresse des Agentenprogs auf den Wert von val, deathproof
	//alle möglichen Fehler hier abfangen!!!
		if (adr>=ptag->memsize) return false;
		if (adr<0) return false;
		ptag->mem[adr]=val;
		return true;
	}
	int getneuePlineNr(const int plineNr, const char add) {
		//wir brauchen hier keine Fehlerauswertung. ungültige Plinenr hat in runprog operate=false zur Folge.
		return (plineNr+add);
	}
//Anfang
	switch (ptvals->befnum){
//              Arithmetik
		case DLBEFADD: //addieren
			if (!setmem(ptag,plineNr*sizeof(progline)+ptvals->ziel, ptvals->op1+ptvals->op2)) return -1;
			plineNr=getneuePlineNr(plineNr, 1);
			break;
		case DLBEFSUB: //subtrahiereh
			if (!setmem(ptag,plineNr*sizeof(progline)+ptvals->ziel, ptvals->op1-ptvals->op2)) return -1;
			plineNr=getneuePlineNr(plineNr, 1);
			break;
		case DLBEFMULT: //mutliplizieren
			if (!setmem(ptag,plineNr*sizeof(progline)+ptvals->ziel, ptvals->op1*ptvals->op2)) return -1;
			plineNr=getneuePlineNr(plineNr, 1);
			break;
		case DLBEFDIV: //dividieren
			if (ptvals->op2==0) return -1; //Division durch null
			if (!setmem(ptag,plineNr*sizeof(progline)+ptvals->ziel, ptvals->op1/ptvals->op2)) return -1;
			plineNr=getneuePlineNr(plineNr, 1);
			break;
		case DLBEFMOD: //modulo
			if (ptvals->op2==0) return -1; //Division durch null
			if (!setmem(ptag,plineNr*sizeof(progline)+ptvals->ziel, ptvals->op1%ptvals->op2)) return -1;
			plineNr=getneuePlineNr(plineNr, 1);
			break;
//      Programmflusskontrolle
		// if springe Programmzeilen vor bzw zurück in r(abh von Modifikator) wenn *op1 = *op2
		case DLBEFIF:
			//ptvals->ziel wird hier (i,j,k) als Programmzeile interpretiert
			if (ptvals->op1==ptvals->op2) plineNr=getneuePlineNr(plineNr, ptvals->ziel); 
			else plineNr=getneuePlineNr(plineNr, 1);
			break;
		case DLBEFIFL:
			// Labeled Jump
			if (ptvals->op1==ptvals->op2) plineNr=findLabel(ptag, ptvals->ziel);
			else plineNr=getneuePlineNr(plineNr, 1);
			break;
		case DLBEFJUMP:
			plineNr=getneuePlineNr(plineNr, ptvals->ziel); 
			break;
		case DLBEFJMPL://labeled jump
			plineNr=findLabel(ptag, ptvals->ziel);
			break;
		//          if springe wenn kleiner:
		case DLBEFIFK:
			if ((unsigned char)ptvals->op1<(unsigned char)ptvals->op2){
				plineNr=getneuePlineNr(plineNr, ptvals->ziel);
				//plineNr=getneuePlineNr(plineNr, zielval/sizeof(progline));
			}
			else plineNr=getneuePlineNr(plineNr, 1);
			break;
		case DLBEFIFKL: //springe wenn kleiner, labeled jump
			if ((unsigned char)ptvals->op1<(unsigned char)ptvals->op2){
				plineNr=findLabel(ptag, ptvals->ziel); 
			}
			else {
				plineNr=getneuePlineNr(plineNr, 1);
			}
			break;
		/*
		DLBEFIFN, //springe wenn nicht gleich
		DLBEFINFL, //springe zu Label wenn nicht gleich
		DLBEFIFG, //springe wenn größer
		DLBEFIFGL, //springe zu Label wenn nicht größer
		*/
		case DLBEFIFN:
			if (ptvals->op1!=ptvals->op2) plineNr=getneuePlineNr(plineNr, ptvals->ziel); 
			else plineNr=getneuePlineNr(plineNr, 1);
			break;
		case DLBEFIFNL:
			if (ptvals->op1!=ptvals->op2) plineNr=findLabel(ptag, ptvals->ziel);
			else plineNr=getneuePlineNr(plineNr, 1);
			break;
		case DLBEFIFG: 
			if ((unsigned char)ptvals->op1>(unsigned char)ptvals->op2){
				plineNr=getneuePlineNr(plineNr, ptvals->ziel);
				//plineNr=getneuePlineNr(plineNr, zielval/sizeof(progline));
			}
			else plineNr=getneuePlineNr(plineNr, 1);
			break;
		case DLBEFIFGL: 
			if ((unsigned char)ptvals->op1>(unsigned char)ptvals->op2){
				plineNr=findLabel(ptag, ptvals->ziel); 
			}
			else {
				plineNr=getneuePlineNr(plineNr, 1);
			}
			break;

		//	Label
		case DLBEFLABL: //labeled jump etc. dh. tue hier nix
			plineNr=getneuePlineNr(plineNr, 1);
			break;
		case DLBEFEND:
			//e ist Programmende für die Progs eines Agenten. 
			plineNr=ende(ptvals, plineNr);
			break;
//      Interaktion, Bewegungssteuerung
		case DLBEFSCAN:
			//          lesen, Umgebung scannen
			//              l   op1=Winkel, op2=Abstand
			//		flag: größere Entfernung
			;
			int dist=glMaxRad;
			if ((ptvals->flags>>6)&0x1) dist=glMaxScanDist;
			if (!simulate) {
				if (!setmem(ptag, plineNr*sizeof(progline)+ptvals->ziel, \
					scan(ptag, ptvals->op1, ptvals->op2, ptevthread->ptobj, dist))) return -1;
				plineNr=getneuePlineNr(plineNr, 1);
			}
			else {
				if (!setmem(ptag, plineNr*sizeof(progline)+ptvals->ziel,0)) return -1;
				plineNr=getneuePlineNr(plineNr, 1);
			}
			break;
		case DLBEFMOVE:
			//          Rennen
			//              r   op1 Winkel op2 Geschwindigkeit
			if (!simulate) {
				if (!setmem(ptag,plineNr*sizeof(progline)+ptvals->ziel,rennen(ptag, ptvals->op1, ptvals->op2, ptevthread->ptobj))) return -1;
			}
			plineNr=getneuePlineNr(plineNr, 1);
			break;
		case DLBEFSPLT:
			//          Zellteilung
			//op1val erbe, op2val Abstand
			if (!simulate) zellteilung(ptag, ptvals->op1, ptvals->op2, ptevthread);
			plineNr=getneuePlineNr(plineNr, 1);
			break;
//	andere Befehle			
		case DLBEFRAND:
			//Zufallszahl
			if (ptvals->op1<ptvals->op2) {
				if (!setmem(ptag,plineNr*sizeof(progline)+ptvals->ziel,g_rand_int_range(glptGrand, ptvals->op1, ptvals->op2))) return -1;
			}
			else if (ptvals->op1>ptvals->op2) {
				if (!setmem(ptag,plineNr*sizeof(progline)+ptvals->ziel,g_rand_int_range(glptGrand, ptvals->op2, ptvals->op1))) return -1;
			}
			else {
				if (!setmem(ptag,plineNr*sizeof(progline)+ptvals->ziel, ptvals->op1)) return -1;
			}
			plineNr=getneuePlineNr(plineNr, 1);
			break;
		case DLBEFSUIC: //lol
			commitsuicide(ptag);
			break;
		case DLBEFNIX: 
			break;
		default :
			plineNr=-1;
	}
	return plineNr;
}

void followFollowedAg(ag *ptag, tpvals *ptvals, int oplineNr, int plineNr, tevthreads *ptthr, int operate) {
//registriere Pfade und mache Konsolenausgaben 
//die globale Variable glfollowedAg hat ptag->agnr des zu folgenden Agenten
//-wenn glfollowedAg==1, dann wurde Agent abgeschaltet. Dieses Signal kann genutzt werden, um die Daten zurückzusetzen
//Definitionen, Variablen
	static GList *pfade=0; //Pfade, die ein Programmlauf durch den Code nimmt
	static int lauf=0;//ein Lauf ist ein ptobj->schritte
	typedef struct {
		char *pfad;
		int *laeufe; //alle Läufe in Reihenfolge, Abschluss immer mit 0x0
	} tepfade;
	static int *executedproglines=0; //Array, der die Anzahl jeder ausgeführten Progline pro Schritt zählt
	static int killpoints=0; //Speicher, um Punktstatistik zu sichern
	static int peacefulpoints=0;
	static long int zeilen=0;
	static char *pfad=0;//String, der für jeden Schritt die ausgeführten Progzeilen notiert
	static int needverbose=0; //wenn gesetzt, ausführliches Protokoll
//Routinen
	void printpfad(char *pfad) {
		for (int i=0; pfad[i]; i++) {
			printf("%3i ", pfad[i]-2);
		}
		printf("\n");
	}
	gint compfunc(tepfade *ptin1, char *ptin2) {
		return strcmp(ptin1->pfad, ptin2);
	}
	void printpfade() {
		GList *ptpf=pfade;
		int i=0;
		while (ptpf) {
			i++;
			int numlaeufe=0;
			while (((tepfade *)(ptpf->data))->laeufe[numlaeufe]) numlaeufe++;
			printf("pfad %3i; läufe: %4i ---> ", i, numlaeufe);
			printpfad(((tepfade *)(ptpf->data))->pfad);
			ptpf=ptpf->next;
		}
	}
	void localrausproc() {
	//wird aufgerufen, um followFollowdAg() zu beenden.
		for (int i=0; i<ptag->memsize/sizeof(progline); i++) executedproglines[i]=0;
		glfollowedAg=0;
		zeilen=0;
		lauf=0;
		pfad=0;
		GList *ptpf=pfade;
		printf("plr \n");
		while (ptpf) {
			free(((tepfade *)(ptpf->data))->pfad);
			free(((tepfade *)(ptpf->data))->laeufe);
			ptpf=ptpf->next;
		}
		g_list_free(pfade);
		pfade=0;
		printf("plr done\n");
	}
//Anfang
return;
	//inits
	g_mutex_lock(&glmZellte); //einfach noch mal benutzen, diesen Lock
	if (executedproglines==0) {
		executedproglines=malloc(sizeof(int)*ptag->memsize/sizeof(progline));
		for (int i=0; i<ptag->memsize/sizeof(progline); i++) executedproglines[i]=0;
	}
	//Pfad sichern in einem String
	//-aktuelle Zeilennummer im aktuellen Pfad sichern
	static bool befissplit=false;//true, wenn der ausgeführte Befehl "splt" ist. dh, wenn einer der Befehle eines Laufs splt ist.
	static int numsplits=0;
	if (operate) {
		executedproglines[oplineNr]++;
		if (pfad==0) {
			befissplit=false; 
			pfad=malloc(2);
			if (!pfad) {
				printf("follow: malloc Kein Speicher\n");
				exit (0);
			}
			pfad[0]=oplineNr+2;
			pfad[1]=0;
		}
		else {
			int strlenpfad=strlen(pfad);
			pfad=realloc(pfad, strlenpfad+2);
			if (!pfad) {
				printf("follow: realloc Kein Speicher\n");
				exit (0);
			}
			pfad[strlenpfad]=oplineNr+2; //es gibt eine Zeile 0, deshalb etwas hinzufügen (damit wir immer einen char-str haben)
			pfad[strlenpfad+1]=0;
		}
		if (ptvals->befnum==DLBEFSPLT) {
			befissplit=true;
			numsplits++;
		}
	}
	if (needverbose) {
		printf("oplineNr %i bef %i %s %i %i %i\n", oplineNr, ptvals->befnum, \
			bef2str(ptvals->befnum), ptvals->op1, ptvals->op2, ptvals->ziel);
		needverbose--;
	}
	//-wenn letzter Befehl, pfad in Liste sichern und pfad auf null setzen für Herstellen von nächstem Pfad
	int treffer=0; //der gesuchte pfad als nummer in der liste pfade
	//Wenn Pfad feststeht, auswerten.
	int numlaeufe=0; //anzahl der läufe (die gespeichert sind in npfad->laeufe) in einem pfad
	if ((plineNr==-1 || plineNr>=ptag->memsize || operate==0) && pfad!=0) {
		lauf++; //erster Lauf trägt lauf=1. Ein lauf ist eigentlich nichts anderes als ein Schritt bzw was während eines Schrittes geschieht
		//Pfad in Liste sichern
		//lauf speichern
		//-prüfen ob neuer Pfad
		GList *ptfund=g_list_find_custom(pfade, pfad,  (GCompareFunc) compfunc);
		//-wenn ja, neuen Pfad anlegen
		if (!ptfund) //g_list_find_custom liefert kein Ergebnis: neuen Pfad anlegen
		{
			tepfade *npfad=malloc(sizeof(tepfade));
			npfad->pfad=pfad;
			npfad->laeufe=malloc(sizeof(int)*2);
			npfad->laeufe[0]=lauf;
			npfad->laeufe[1]=0;
			pfade=g_list_append(pfade, npfad);
			printf("\n");
			printf("pfad %i:  ", g_list_length(pfade));
			printpfad(pfad);
			numlaeufe=0; //läufe dieses Pfades zählen
			while (npfad->laeufe[numlaeufe]) numlaeufe++;
			treffer=g_list_length(pfade);
		}
		//-wenn nein, lauf hinzufügen
		else {
			free(pfad);
			//numlaeufe=0; //läufe dieses Pfades
			//--Anzahl Läufe finden: Ende von Einträgen der Läufe finden
			while (((tepfade *)(ptfund->data))->laeufe[numlaeufe]) numlaeufe++; 
			//--neuen Speicherplatz für Lauf registrieren
			((tepfade *)(ptfund->data))->laeufe = realloc(((tepfade *)(ptfund->data))->laeufe, (numlaeufe+2)*sizeof(int)); 
			if (!((tepfade *)(ptfund->data))->laeufe) {
				printf("follow realloc laeufe: kein Speicher\n");
				exit (0);
			}
			((tepfade *)(ptfund->data))->laeufe[numlaeufe]=lauf; //da erster Lauf auf [0] geschrieben wird
			((tepfade *)(ptfund->data))->laeufe[numlaeufe+1]=0; //null setzen, damit Ende gefunden werden kann
			numlaeufe++;//einer wurde hinzugefügt
			treffer=g_list_position(pfade, ptfund)+1;
		}
		if (befissplit) printf("*");
		printf("%i ", treffer);
		fflush(stdout);
		pfad=0;
	}
	else if (!pfad){
		printf("0");
		//printf("oplineNr %i bef %i %s %i %i %i\n", oplineNr, ptvals->befnum, bef2str(ptvals->befnum), ptvals->op1, ptvals->op2, ptvals->ziel);
		//needverbose=100;
		fflush(stdout);
	}
	zeilen++;
	//jeder Aufruf der Prozedur followFollowedAg() wird hier wieder abgeschaltet
	//hier kann ein destruktor programmiert werden
	//- hier auch: Ausnahmebehandlung: Agent wurde abgeschaltet
	if (zeilen>100000 || glfollowedAg==1){//|| strlen(pfad)>1000) {
		if (!pfad) {
			printf("Pfadproblem in followFollowedAg. Auswertung abgebrochen\n");
			localrausproc();
			g_mutex_unlock(&glmZellte);
			return;
		}
		if (strlen(pfad)>1000) {
			printf("Pfad zu lang\n");
		}
		else {
			printf(">>>>\n");
			if (glfollowedAg!=1) printf("fertig.\n");
			printpfade();
			printf("splits: %i\n", numsplits);
		}
		numsplits=0;
		int numex=0;
		for (int i=0; i<ptag->memsize/sizeof(progline); i++) {
			//printf("%i %i\n", i, executedproglines[i]);
			if (executedproglines[i]) numex++;
		}
		printf("Gesamtzahl Progzeilen: %li. Executed proglines: %i\n", ptag->memsize/sizeof(progline), numex);
		printf("Anzahl Schritte %i\n", lauf);
		printf("Gesamtzahl während der Beobachtung abgearbeitete Proglines %li\n", zeilen);
		if (lauf) printf("Anzahl Proglines pro Schritt %f\n",(float)zeilen/lauf);
		printf("Killpoints %i peacefulpoints %i\n", killpoints, peacefulpoints);
		localrausproc();
	}
	else {
		//Punktstatistik sichern für Ausgabe, wenn Agent abgeschaltet wurde
		killpoints=ptag->killpoints;
		peacefulpoints=ptag->peacefulpoints;
	}
	//zu lange Pfade
	g_mutex_unlock(&glmZellte);
}

GFunc runprog(ag *ptag, tevthreads *ptthstruct){
//Programm des Agenten ptag abarbeiten
	int plineNr=0;
	int oplineNr=plineNr;
	bool operate=true;
	tpvals vals; //die operativen Werte in diese Struktur schreiben, dann runprogline aufrufen
	tpvals *ptvals=&vals;
	if (ptag->sterben) operate=false;
	while (operate) {
		ptag->linesdone++;
		//errechne Werte für runprogline, vor allem: bearbeite Flags
		progline *ptprogline=(progline*)ptag->mem+plineNr;
		ptvals->befnum=ptprogline->bef;
		ptvals->flags=ptprogline->flags;
		//move und scan sollen op1 nicht mit höherem Flag auswerten (dh Sonderfunktionen)
		//-das wurde eingeführt, weil die Agenten sonst "wedeln", was ein zu einfacher escape ist
		if (ptvals->befnum==DLBEFMOVE || ptvals->befnum==DLBEFSCAN) {
			if (!getVal(&ptvals->op1, ptprogline->op1, (ptvals->flags>>4)&0x1, 0, (char *) ptprogline, ptag)) {
				operate=false;
				goto kosten;
			}
			if (!getVal(&ptvals->op2, ptprogline->op2, (ptvals->flags>>2)&0x1, 0, (char *) ptprogline, ptag)) {
				operate=false;
				goto kosten;
			}
			if (!getZiel(&ptvals->ziel, ptvals->flags&0x1, ptprogline->ziel, (char *) ptprogline, ptag)) {
				operate=false;
			}
		}
		//fälle, in denen op1, op2 und ziel nicht ausgewertet werden
		else if (ptvals->befnum>=DLBEFLAST || ptvals->befnum==DLBEFEND) goto kosten;
		//fälle, in denen nur op1 ausgewertet wird
		else if (ptvals->befnum==DLBEFLABL) {
			if (!getVal(&ptvals->op1, ptprogline->op1, (ptvals->flags>>4)&0x1, (ptvals->flags>>5)&0x1, (char *) ptprogline, ptag)) {
				operate=false;
			}
		}
		//fälle, in denen nur ziel ausgewertet wird
		else if (ptvals->befnum==DLBEFJUMP || ptvals->befnum==DLBEFJMPL) {
			if (!getZiel(&ptvals->ziel, ptvals->flags&0x1, ptprogline->ziel, (char *) ptprogline, ptag)) {
				operate=false;
			}
		}
		//fälle, in denen nur op1 und op2 ausgewertet werden
		else if (ptvals->befnum==DLBEFSPLT) {
			if (!getVal(&ptvals->op1, ptprogline->op1, (ptvals->flags>>4)&0x1, (ptvals->flags>>5)&0x1, (char *) ptprogline, ptag)) {
				operate=false;
				goto kosten;
			}
			if (!getVal(&ptvals->op2, ptprogline->op2, (ptvals->flags>>2)&0x1, (ptvals->flags>>3)&0x1, (char *) ptprogline, ptag)) {
				operate=false;
			}
		}
		//alle anderen
		else {
			if (!getVal(&ptvals->op1, ptprogline->op1, (ptvals->flags>>4)&0x1, (ptvals->flags>>5)&0x1, (char *) ptprogline, ptag)) {
				operate=false;
				goto kosten;
			}
			if (!getVal(&ptvals->op2, ptprogline->op2, (ptvals->flags>>2)&0x1, (ptvals->flags>>3)&0x1, (char *) ptprogline, ptag)) {
				operate=false;
				goto kosten;
			}
				//bool getZiel(char *raus, char mod, char val, void *ptcprogline, ag *ptag) {
			if (!getZiel(&ptvals->ziel, ptvals->flags&0x1, ptprogline->ziel, (char *) ptprogline, ptag)) {
				operate=false;
			}
		}
		//Kosten, Beendigungen, Ausführen, sonstiges
		int ckosten;
		kosten:
		ckosten=kosten(ptag, ptvals->befnum, ptvals->op1, ptvals->op2);
		ptthstruct->addtomatrix-=ckosten;
		ptthstruct->ckosten+=ckosten;
		if (!operate) goto endwhile;
		//-ist Agent schon tot?
		if (ptag->ptworld->Nutri==0 || ptag->sterben) {
			operate=false;
			goto endwhile;
		}
		//-Schutz gegen Überlauf von ->Nutri, zu fette Agenten abschalten
		if (ptag->ptworld->Nutri>60000) {
			//printf("ugh\n");
			ptag->sterben=1; 
		}
		oplineNr=plineNr; //für followFollowedAg()
		//-here it comes... here it comes...
		plineNr=runprogline(ptag, ptvals, plineNr, ptthstruct, 0);
		//-Abbruchbedingungen nach plineNr
		if (plineNr>=ptag->memsize/sizeof(progline)) { 
			operate=false; 
		}
		else if (plineNr<0) {
			operate=false;
		}
		endwhile:
		if (ptag->agnr==glfollowedAg || glfollowedAg==1) {
			followFollowedAg(ptag, ptvals, oplineNr, plineNr, ptthstruct, operate);
		}
	}
	return 0;
}
 
void printProg(char *prog, int size, CallbackObject *ptobj) {
//ein Programm auf der Konsole ausgeben
	char getZeile(char val, char currentline) {
		char sizeofprogline=sizeof(progline); //komischer bug mit division bei (char)zahl/sizeof(progline)
		char zeile=val/sizeofprogline;
		//if (val<0) zeile=zeile-1; das ist wohl einfach falsch
		zeile=zeile+currentline; 
		return zeile;
	}
	char getZelle(char val) {
		char sizeofprogline=sizeof(progline); //komischer bug mit division bei (char)zahl/sizeof(progline)
		char zelle=val%sizeofprogline;
		if (zelle<0) zelle=zelle+5; 
		return zelle;
	}
//Anfang
	ag *ptag=g_new(ag,1);
	ptag->memsize=size;
	ptag->mem=prog;
	ptag->ptworld=(worldbit *)&world;
	ptag->subx=0;
	ptag->suby=0;
	tpvals vals;
	tpvals *ptvals=&vals;
	//durch alle vollständigen Programmzeilen gehen und ausdrucken
	for (int i=0; i<size/sizeof(progline); i++) {
		//Werte für op1 etc ermitteln
		progline *ptprog=(progline *)prog+i;
		bool executable=1;
		int nextprogline;
		int sprungadresse2;
		ptvals->befnum=ptprog->bef;
		ptvals->flags=ptprog->flags;
		if (ptvals->befnum==DLBEFMOVE || ptvals->befnum==DLBEFSCAN) {
			if (!getVal(&ptvals->op1, ptprog->op1, (ptvals->flags>>4)&0x1, 0, (char *) ptprog, ptag)) {
				executable=false;
			}
			if (!getVal(&ptvals->op2, ptprog->op2, (ptvals->flags>>2)&0x1, 0, (char *) ptprog, ptag)) {
				executable=false;
			}
			if (!getZiel(&ptvals->ziel, ptvals->flags&0x1, ptprog->ziel, (char *) ptprog, ptag)) {
				executable=false;
			}
		}
		else {
			if (!getVal(&ptvals->op1, ptprog->op1, (ptvals->flags>>4)&0x1, (ptvals->flags>>5)&0x1, (char *) ptprog,ptag)) executable=0;
			if (!getVal(&ptvals->op2, ptprog->op2, (ptvals->flags>>2)&0x1, (ptvals->flags>>3)&0x1, (char *) ptprog,ptag)) executable=0;
			if (!getZiel(&ptvals->ziel, (ptvals->flags>>0)&0x1, ptprog->ziel, ptprog, ptag)) executable=0;
		}
		//-nächste Programmzeile ermitteln
		if (executable) nextprogline=runprogline(ptag, ptvals, i, 0, 1);
		else {
			nextprogline=-1;
		}
		char *str=bef2str(ptvals->befnum);
		if (ptvals->befnum==DLBEFIF || ptvals->befnum==DLBEFIFK|| \
			ptvals->befnum==DLBEFJUMP) {
			sprungadresse2=ptvals->ziel+i;
		}
		else if (ptvals->befnum==DLBEFIFL|| ptvals->befnum==DLBEFIFKL|| \
			ptvals->befnum==DLBEFJMPL) {
			sprungadresse2=findLabel(ptag,ptvals->ziel);
		}
		else {
			sprungadresse2=0;
		}
		//raw
		printf("%2i %3i %3i    %3i    %3i    %3i \n", i, (unsigned char) ptprog->bef, (unsigned char) ptprog->flags, \
			(unsigned char) ptprog->op1, (unsigned char) ptprog->op2, (unsigned char) ptprog->ziel);
		//zweite Zeile, aufbearbeitete Daten
		//-print Flags für Bef, bef
		printf("   %i%i|%4s ", ptvals->flags>>7&0x1, ptvals->flags>>6&0x1, str);
		//-print flags für op1, op1
		printf("%i%i|%3i ", ptvals->flags>>5&0x1, ptvals->flags>>4&0x1, (unsigned char)ptvals->op1);
		//-print flags für op2, op2
		printf("%i%i|%3i ", ptvals->flags>>3&0x1, ptvals->flags>>2&0x1, (unsigned char)ptvals->op2);
		//-print flags für ziel, ziel
		printf("%i%i|%3i ", ptvals->flags>>1&0x1, ptvals->flags>>0&0x1, (unsigned char)ptvals->ziel);
		//-executable, nextProgline
		printf("%i %3i ", executable, nextprogline);
		//-nächste alternative Sprungzeile
		if (ptvals->befnum==DLBEFIF || ptvals->befnum==DLBEFIFL|| ptvals->befnum==DLBEFIFK|| ptvals->befnum==DLBEFIFKL|| \
			ptvals->befnum==DLBEFJUMP || ptvals->befnum==DLBEFJMPL) {
			printf("%3i ", sprungadresse2);
		}
		else {
			//Lücke auffüllen
			printf("    ");
		}
		//selbe Zeile, ermitteln der Adressen
		//-Zeile und Zelle für op1, op2, ziel
		//-op1 und op2 aber nur, wenn indirect-flag gesetzt ist
		if (ptvals->flags>>4&0x1) {
			printf("%3i,%i ", getZeile(ptprog->op1,i), getZelle(ptprog->op1));
		}
		else {
			printf("  val ");
		}
		if (ptvals->flags>>2&0x1) {
			printf("%3i,%i ", getZeile(ptprog->op2,i), getZelle(ptprog->op2));
		}
		else {
			printf("  val ");
		}
		//ziel 
		//-aber nur, wenn an ziel auch wirklich geschrieben wird, dh nicht bei zb. DLBEFSPLT
		if (ptvals->befnum==DLBEFADD || ptvals->befnum==DLBEFSUB  || ptvals->befnum==DLBEFMULT || ptvals->befnum==DLBEFDIV || \
			ptvals->befnum==DLBEFMOD || ptvals->befnum==DLBEFIF || ptvals->befnum==DLBEFIFL || ptvals->befnum==DLBEFIFK || \
			ptvals->befnum==DLBEFIFKL || ptvals->befnum==DLBEFIFK || ptvals->befnum==DLBEFIFKL || ptvals->befnum==DLBEFJUMP || \
			ptvals->befnum==DLBEFJMPL || ptvals->befnum==DLBEFMOVE  || ptvals->befnum==DLBEFSCAN  || ptvals->befnum==DLBEFRAND  ||
			ptvals->befnum==DLBEFIFN || ptvals->befnum==DLBEFIFNL || ptvals->befnum==DLBEFIFG || ptvals->befnum==DLBEFIFGL) {
			//ziel ist Sprungadresse, nicht Schreibziel
			if (ptvals->befnum==DLBEFIF || ptvals->befnum==DLBEFIFL || ptvals->befnum==DLBEFIFK || \
				ptvals->befnum==DLBEFIFKL || ptvals->befnum==DLBEFJUMP || ptvals->befnum==DLBEFJMPL ||
				ptvals->befnum==DLBEFIFN || ptvals->befnum==DLBEFIFNL || ptvals->befnum==DLBEFIFG || ptvals->befnum==DLBEFIFGL) {
				printf("%5i ", ptvals->ziel+i);

			}
			//Speicherplatz der Adresse des Ziels als Zeile,Zelle
			else if (ptvals->flags&0x1) printf("%3i,%i:", getZeile(ptprog->ziel,i), getZelle(ptprog->ziel));
			else printf("      ");
			printf("%3i,%i ", getZeile(ptvals->ziel,i), getZelle(ptvals->ziel));
		}
		//es wird nicht an Adresse geschrieben, deshalb none
		else printf(" none ");
		printf("\n");
		if (nextprogline>size/sizeof(progline)) nextprogline=-999;
	}
}

void translateProgFromString(char *ptmem) {
//string rein(glInitProg), prog in ptag einfügen
    gchar *str=g_new(gchar,strlen(glInitProg)+1);
    strcpy(str,glInitProg);
    progline *ptprog=(progline*) ptmem;
    char *ptr=strtok(str," ");
    int i=0;
    progline *schreibstelle=ptprog;
    while (ptr!=NULL) {
        switch (i%5) {
            case 0: //befehl
	    	//printf("tpfs %s\n", ptr);
                schreibstelle->bef=str2bef(ptr);
		//printf("tr %i\n", schreibstelle->bef);
  		break;
            case 1: //die Flags
                if (*ptr!='0') schreibstelle->flags|=(1<<7); //ja ist schon so
		else schreibstelle->flags&=~(1<<7);
                if (*(ptr+1)!='0') schreibstelle->flags|=(1<<6);
		else schreibstelle->flags&=~(1<<6);
                if (*(ptr+2)!='0') schreibstelle->flags|=(1<<5);
		else schreibstelle->flags&=~(1<<5);
                if (*(ptr+3)!='0') schreibstelle->flags|=(1<<4);
		else schreibstelle->flags&=~(1<<4);
                if (*(ptr+4)!='0') schreibstelle->flags|=(1<<3);
		else schreibstelle->flags&=~(1<<3);
                if (*(ptr+5)!='0') schreibstelle->flags|=(1<<2);
		else schreibstelle->flags&=~(1<<2);
                if (*(ptr+6)!='0') schreibstelle->flags|=(1<<1);
		else schreibstelle->flags&=~(1<<1);
                if (*(ptr+7)!='0') schreibstelle->flags|=1;
		else schreibstelle->flags&=~(1);
            	break;
            case 2:
                schreibstelle->op1=(short)g_ascii_strtoull(ptr, 0, 10);
            	break;
            case 3:
                schreibstelle->op2=(short)g_ascii_strtoull(ptr, 0, 10);
		break;
            case 4:
                schreibstelle->ziel=(short)g_ascii_strtoull(ptr, 0, 10);
                schreibstelle++;
        }
        i++;
        ptr=strtok(NULL," ");
    }
    //memcpy(ptag->mem+160,"\0\0\0\0\0\0\0\0",8);
    g_free(str);
    //printProg(ptmem,glMemSize, 0);
}
 
void createProg(ag *ptag) {
//Programm glInitProg übersetzen und verwenden 
	char* ptprog=g_new(char, ptag->memsize);
	for (int i=0; i<ptag->memsize; i++) ptprog[i]=(char)g_rand_int_range(glptGrand,0,255);
	translateProgFromString(ptprog);
	ptag->mem=ptprog;
}

void createRndProg(ag *ptag) {
//Zufallsprogramm kreieren
	int size=ptag->memsize/sizeof(progline);
	ptag->mem=g_new(char, glMemSize);
	if (!ptag->mem) {
		printf("Fehler in createRndProg: Kein Speicher\n");
		exit (0);
	}
	for (int i=0; i<size; i++) {
		progline *ptpl=createRndProgline(ptag, i);
		memcpy(ptag->mem+i*sizeof(progline), ptpl, sizeof(progline));
		g_free(ptpl);
	}
}


void initAgs(CallbackObject *ptobj, bool verbose) {
//initiiere Agenten gemäß signifikanter Variablen
	void initAgProg(ag *ptag, int num) {
	//initiiere Programm eines Agenten gemäß signifikanter Variablen
		void printcolor(char *name, color color) {
			printf("color %s %i %i %i\n", name, color.red, color.green, color.blue);
		}
		void createProgFromBestAgs(ag *ptag) {
		//schreibe Programm aus der Datei bester Agenten in ptag->mem
			float numpersam=(float)(glNumAgents+1)/glExeBest;
			if (!numpersam) {
				printf("glNumAgents / glExeBest < 1. Tschüss\n");
				exit (0);
			}
			GList *samptlag=glBestAgs; //glBestAgs wird eingelesen in leseAlltimeBestAgentsFile()
			int numsamp=(int)((float)g_list_length(glptagFromInitAgs)/numpersam);
			for (int i=0; i<numsamp; i++) samptlag=samptlag->next;
			ptag->memsize=((ag*)(samptlag->data))->memsize;
			ptag->mem=g_memdup(((ag*)(samptlag->data))->mem, ptag->memsize);
			ptag->Signatur=memcpy(ptag->Signatur, ((ag*)(samptlag->data))->Signatur, glSigsize);
			memcpy(&ptag->bocolor,&((ag*)(samptlag->data))->bocolor, sizeof(color));
			memcpy(&ptag->iycolor,&((ag*)(samptlag->data))->iycolor, sizeof(color));
			memcpy(&ptag->iyfcolor,&((ag*)(samptlag->data))->iyfcolor, sizeof(color));
			memcpy(&ptag->bofcolor,&((ag*)(samptlag->data))->bofcolor, sizeof(color));
			memcpy(&ptag->schleimspur,&((ag*)(samptlag->data))->schleimspur, sizeof(color));
		}

	//Anfang
		static char *currprog=0;
		static int cprogsize=0;
		if (glRndProgs && glTestlaufBestAgs) {
			printf("glRndProgs und glTestlaufBestAgs gesetzt.\n   Da weiß ich nicht, was ich machen soll\n   Tschüss.\n");
			exit (0);
		}
		else if (glRndProgs) {
			ptag->memsize=glMemSize;
			createRndProg(ptag);
			char puf[100];
			sprintf(puf,"xharx$r%i", num);
			ptag->Signatur=memcpy(ptag->Signatur, puf, 100);
		}
		else if (glExeBest) {
			createProgFromBestAgs(ptag);
		}
		else if (glTestlaufBestAgs) {
			if (num==1) {
				if (currprog) g_free(currprog);
				if (g_list_length(glBestAgs)==0) {
					printf("glTestlaufBestAgs: letzter Agent abgeschlossen. Gut.\n");
					exit (0);
				}
				printf("   Neuer Lauf mit bestag %i, sig=%s\n", g_list_length(glBestAgs), ((ag*)(glBestAgs->data))->Signatur);
				currprog=((ag*)(glBestAgs->data))->mem;
				cprogsize=((ag*)(glBestAgs->data))->memsize;
				ptag->mem=g_memdup(currprog, cprogsize);
				ptag->memsize=cprogsize;
				ptag->Signatur=memcpy(ptag->Signatur,((ag*)(glBestAgs->data))->Signatur, glSigsize);
				memcpy(&ptag->bocolor,&((ag*)(glBestAgs->data))->bocolor, sizeof(color));
				memcpy(&ptag->iycolor,&((ag*)(glBestAgs->data))->iycolor, sizeof(color));
				memcpy(&ptag->iyfcolor,&((ag*)(glBestAgs->data))->iyfcolor, sizeof(color));
				memcpy(&ptag->schleimspur,&((ag*)(glBestAgs->data))->schleimspur, sizeof(color));
				memcpy(&ptag->bofcolor,&((ag*)(glBestAgs->data))->bofcolor, sizeof(color));
				printf("%s\n", ptag->Signatur);
				printcolor("bololor", ptag->bocolor);
				printcolor("iycolor", ptag->iycolor);
				printcolor("iyfcolor", ptag->iyfcolor);
				printcolor("bofcolor", ptag->bofcolor);
				printcolor("schleimspur", ptag->schleimspur);
				if (verbose) printProg(ptag->mem, ptag->memsize, ptobj);
				if (verbose) printf("memsize %i\n", ptag->memsize);
				glBestAgs=g_list_remove_link(glBestAgs, glBestAgs);
			}
			else {
				ptag->mem=g_memdup(currprog, cprogsize);
				//printf("%p %p\n", currprog, ptag->mem);
				ptag->memsize=cprogsize;
			}
		}
		else {
			ptag->memsize=glMemSize;
			ptag->Signatur=memcpy(ptag->Signatur,"xharx", 6);
			createProg(ptag);
		}
	}
//Anfang
	printf("initAgs ... ");
	int i=0;
	if  (!glRndProgs) printf("\n   glRndProgs ist off\n");
	if (glTestlaufBestAgs) printf("   glTestlaufBestAgs in on\n");
	if (glExeBest) printf("   glExebest ist on\n");
	else printf("   glExeBest ist off\n");
	for (i=1; i<=glNumAgents; i++) {
		//Matarixjobs
		int xr, yr;
		do {
			xr=g_rand_int_range(glptGrand,0,glWorldsize);
			yr=g_rand_int_range(glptGrand,0,glWorldsize);
			//xr=((long)rand()*glWorldsize)/RAND_MAX;
			//yr=((long)rand()*glWorldsize)/RAND_MAX;
		} while ((world+xr+(yr*glWorldsize))->Agind!=0);
		long aug=(xr+yr*glWorldsize);
		//allozieren
		ag* ptag=allocAg();
		if (!ptag) {
			printf("initAgs: kein ptag\n");
			exit (0);
		}
		//matrix updaten
		ptag->ptworld=world+aug;
		ptag->ptworld->Nutri+=glValAgents;
		glMatrixpoints+=glValAgents; //muss nicht gelockt werden, prozedur ist nicht multithreaded
		ptag->ptworld->Agind=getAgind(ptag);
		//Agenten einklinken
		glptagFromInitAgs=g_list_prepend(glptagFromInitAgs, ptag);
		//agenten initialisieren
		ptag->generationenZ=0;
		ptag->generationenMutZ=0;
		ptag->agnr=glZLfNr++;
		ptag->linesdone=0;
		ptag->subx=0;
		ptag->suby=0;
		ptag->sterben=0;
		ptag->geburt=0;
		ptag->bocolor=agcolor;
		ptag->iycolor=lesencolor;
		ptag->iyfcolor=lesenlongdistcolor;
		ptag->bofcolor=agcolorfug;
		ptag->schleimspur=schleimspur;
		ptag->changecolorhow=0;
		ptag->ctrresize=0;
		//-Prog creieren
		initAgProg(ptag, i);
		if (ptag->mem==NULL) {
			printf("Fehler beim Erstellen der Agenten: ->mem == NULL\n");
			exit (0);
		}
		//-etc
		ptobj->anz++;
	}
	printf("   done initAgs. i=%i\n", i-1);
}

void initdistro() {
//initiiere Array, der Indexe auf freie Agentenslots verwaltet
	for (int i=(1<<16)-1; i>0; i--) {
		ptdistri=g_slist_prepend(ptdistri,GINT_TO_POINTER(i));
	}
	printf("done initdistro.\n");
}

void paintRuler(CallbackObject *ptobj) {
	//horizontalen Ruler zeichnen
	if (ptobj->yorig>glWorldsize-ptobj->hoehe) {
		for (gint i=0; i<ptobj->breite; i++) {
			tpaintlist plistel;
			plistel.x=i;
			plistel.y=glWorldsize-ptobj->yorig;
			plistel.type=DLCLTRULER;
			appendToPlistOnce(&plistel, ptobj);
		}
	}
	//vertikalen
	if (ptobj->xorig>glWorldsize-ptobj->breite) {
		for (gint i=0; i<ptobj->hoehe; i++) {
			tpaintlist plistel;
			plistel.x=glWorldsize-ptobj->xorig;
			plistel.y=i;
			plistel.type=DLCLTRULER;
			appendToPlistOnce(&plistel, ptobj);
		}
	}
}

void initarena(CallbackObject *ptobj) {
//fülle ptobj->daten mit Informationen aus der Matrix (world)
	g_slist_foreach(ptobj->ptplist,(GFunc)g_free,NULL);
	ptobj->ptplist=NULL;
	g_slist_foreach(ptobj->ptnplist,(GFunc)g_free,NULL);
	ptobj->ptnplist=NULL;
	paintRuler(ptobj);
	gint xorig=ptobj->xorig%glWorldsize;
	gint yorig=ptobj->yorig%glWorldsize;
	while (xorig<0)xorig+=glWorldsize;
	while (yorig<0)yorig+=glWorldsize;
	for (int i=0; i<ptobj->breite; i++) {
		for (int j=0; j<ptobj->hoehe; j++) {
			/*
			tpaintlist *ptp=malloc(sizeof(tpaintlist));
			ptp->x=i;
			ptp->y=j;
			*/
			if ((world+(i+xorig)%glWorldsize+((j+yorig)%glWorldsize)*glWorldsize)->Nutri) {
				//ptp->type=DLCLTRAIN;
				//wir könnten es auch mit appendToPList() machen, zu langsam 
				ptobj->daten[j*ptobj->breite*3+i*3+0]=raincolor.red;
				ptobj->daten[j*ptobj->breite*3+i*3+1]=raincolor.green;
				ptobj->daten[j*ptobj->breite*3+i*3+2]=raincolor.blue;
			}
			else {
				//ptp->type=DLCLTNULL;
				ptobj->daten[j*ptobj->breite*3+i*3+0]=nullcolor.red;
				ptobj->daten[j*ptobj->breite*3+i*3+1]=nullcolor.green;
				ptobj->daten[j*ptobj->breite*3+i*3+2]=nullcolor.blue;
			}
			//appendToPlistOnce(ptp, ptobj);
		}
	}
}

void initWorld() {
//alloziere Speicherplatz für die Matrix und initialisiere auf 0
	printf("initWorld\n");
	if (!world) world=g_new(worldbit,glWorldsize*glWorldsize);
	if (world==NULL) {
		printf("zu wenig Speicher. Versuch es vielleicht gleich noch einmal. Tschüss.\n");
		exit (0);
	}
	else {
		printf("   Das hat geklappt. %i Gitterpunkte insgesamt, %li Speicherbedarf\n", \
			glWorldsize*glWorldsize, glWorldsize*glWorldsize*sizeof(worldbit));
	}
	printf("   world= %p set all fields of matrix 0 ...  ",world);
	for (int i=0; i<glWorldsize; i++) {
		for (int j=0; j<glWorldsize; j++) {
			(world+i*glWorldsize+j)->Nutri=0;
			(world+i*glWorldsize+j)->Agind=0;
		}
	}
	printf("done\n");
}

gboolean readOpts(char *optsfile, char *schwanz, gpointer data, GError **error) {
	//if (strcmp(schwanz,"")==0) {
	//	printf("kein Dateiname");
	//}
	//else printf("Optionen aus Datei lesen %s\n", schwanz);
	printf("Optionen aus Datei lesen %s. (Noch nicht ausgeführt)\n", schwanz);
	exit (0);
	return true;
}

gboolean saveOpts(char *bef, char *schwanz, gpointer data, GError **error) {
	//char *filename;
	//if (strcmp(schwanz,"")==0) filename=glOptsfileName;
	//else filename=schwanz;
	//FILE *datei=fopen(filename, "+w");
	printf("Optionen schreiben %s. (Noch nicht ausgeführt)\n", schwanz);
	exit (0);
	return true;
}

void leseAlltimeBestAgentsFile(CallbackObject *ptobj, char mode) {
//mode==1 gebe Programme der best agents auf der Konsole aus
//mode==2 gebe Liste aus
//mode==3 erstelle Liste mit Agenten auf glptevthreads[0]->ptfirstag
//mode==4 erstelle Liste aller Agenten in glBestAgs
	bool agentFitsToTimestamp(ag *ptag) {
		if (strlen(glExeBFirstDate)==0) return true;
		if (strcmp(glExeBFirstDate, ptag->datum)<=0) return true;
		return false;
	}
	FILE *datei=fopen(glAlltimeBestAgentsFile, "r");
	if (!datei) {
		printf("Datei nicht gefunden: %s\n", glAlltimeBestAgentsFile);
		exit (0);
	}
	//bestimme Größe der Datei
	fseek(datei,0, SEEK_END);
	long fsize=ftell(datei);
	fseek(datei,0l, SEEK_SET);
	int kopfsize;
	int memsize;
	tbestAgentKopf kopf;
	int kmyversion; 
	int kmemsize;
	char *ksignatur;
	int num=0;
	int anz=0;
	int anzfits=0;
	while (ftell(datei)<fsize) {
		num++;
		fread(&kopfsize,sizeof(int),1, datei);
		fread(&kopf, kopfsize, 1, datei);
		kmyversion=kopf.myversion;
		kmemsize=kopf.memsize;
		ksignatur=kopf.signatur;
		fread(&memsize, sizeof(int), 1, datei);
		char prog[memsize];
		fread(prog, memsize, 1, datei);
		if (mode==5) printf("Agent #%i memsize=%i ver=%i signatur=%s\n", num, kmemsize, kmyversion, ksignatur); //ausgeschaltet 
		if (mode==1) printProg(prog, memsize, ptobj);
		if (mode==3 || mode==4) {
			if (!memsize) {
				printf("Agent (num %i) wurde nicht eingelesen, memsize == 0\n", num);
				continue;
			}
			ag *ptag=allocAg();
			if (!ptag) {
				printf("lesealltimeBestAgentsFile: Kein ptag.\n"); //###
				exit (0);
			}
			//Farben einlesen
			if (kopf.myversion<4) {
				ptag->bocolor=kopf.bocolor;
				ptag->iycolor=kopf.iycolor;
				ptag->iyfcolor=kopf.iyfcolor;
				ptag->bofcolor=kopf.bofcolor;
				ptag->schleimspur=schleimspur;
			}
			if (kopf.myversion==4) {
				ptag->bocolor=kopf.bocolor;
				ptag->iycolor=kopf.iycolor;
				ptag->iyfcolor=kopf.iyfcolor;
				ptag->bofcolor=kopf.bofcolor;
				ptag->schleimspur=kopf.schleimspur;
			}
			strcpy(ptag->datum, kopf.datum);
			//einklinken in Liste der besten Agenten
			if (agentFitsToTimestamp(ptag)) {
				ptag->agnr=++anzfits;
				glBestAgs=g_list_prepend(glBestAgs, ptag);
			}
			//Werte setzen
			ptag->memsize=memsize;
			ptag->mem=g_memdup(prog,ptag->memsize);
			if (!ptag->mem) {
				printf("leseAlltimeBestAgentsFile. Kein mem\n");
				exit (0);
			}
			memcpy(ptag->Signatur, ksignatur, glSigsize);
			anz++;
		}
	}
	if (!glExeBest && sizeof(glExeBFirstDate)) {
		glExeBest=anzfits;
	}
	if (mode==3) {
		printf("Beste Agenten eingelesen, gesamt %i Agenten\n", num);
		printf("   passende Progs gefunden= %i\n", g_list_length(glBestAgs));
		printf("   glExeBest = %i, glNumAgents = %i \n",glExeBest, glNumAgents);
	}
	if (mode==4) printf("Beste %i Agenten für Testläufe eingelesen\n", anz);
}

gboolean clineOther(char *name, char *arg, CallbackObject *ptobj){
//"andere Befehle" der Commandline auswerten
	if (!strcmp(name, "--print")) {
		printf("print\n");
		if (arg && !strcmp(arg,"heads")) {
			leseAlltimeBestAgentsFile(ptobj, 2);
			exit (0);
		}
		leseAlltimeBestAgentsFile(ptobj, 1);
		exit (0);
	}
	else if (!strcmp(name, "--glExeBest")) {
		glExeBest=atoi(arg);
		printf("Lauf mit besten Agenten: %s %i\n", name, glExeBest);
	}
	else if (!strcmp(name, "--glExeBFirstDate")) {
		if (strlen(arg)>=sizeof(glExeBFirstDate)) {
			printf("clineOther: Argument --glExeBFirstDate zu lang\n");
			exit (0);
		}
		strcpy(glExeBFirstDate, arg);
		//Wert korrigieren, wenn nötig
		if (strlen(glExeBFirstDate)<sizeof(glExeBFirstDate)) {
			for (int i=strlen(glExeBFirstDate); i<=sizeof(glExeBFirstDate); i++) {
				glExeBFirstDate[i]='0';
			}
			glExeBFirstDate[sizeof(glExeBFirstDate)-1]=0;
		}
	}
	return true;
}

void parseCommandline(int argc, char **argvo) {
	static GOptionEntry entries[] = {
		{ "glWorldsize", 0, 0, G_OPTION_ARG_INT, &glWorldsize, "Größe der Matrix in x- und y- Richtung", "THZE" },
		{ "glNumRegen", 0, 0, G_OPTION_ARG_INT, &(glNumRegen), "Menge an Regen, der bei jedem Schritt fallen soll", NULL },
		{ "glValRegen", 0, 0, G_OPTION_ARG_INT, &(glValRegen), "Punktwert eines Regentropfens", NULL },
		{ "glNumAgents", 0, 0, G_OPTION_ARG_INT, &(glNumAgents), "Anzahl der Agenten bei Programmstart", NULL },
		{ "glValAgents", 0, 0, G_OPTION_ARG_INT, &(glValAgents), "Punkte der Agenten bei Programmstart", NULL },
		{ "glInitDrops", 0, 0, G_OPTION_ARG_INT, &(glInitDrops), "Anzahl der Regentropfen vor erstem Schritt", NULL },
		{ "glRndProgs", 0, 0, G_OPTION_ARG_INT, &(glRndProgs), "Erstelle Zufallsprogramme beim Start, sonst glInitProg oder default", NULL },
		{ "glNumThreads", 0, 0, G_OPTION_ARG_INT, &(glNumThreads), "Erstelle Zufallsprogramme beim Start, sonst glInitProg oder default", NULL },
		{ "glZLfNr", 0, 0, G_OPTION_ARG_INT, &(glZLfNr), "Laufnummer des ersten Agentens", NULL },
		{ "glInitProg", 0, 0, G_OPTION_ARG_STRING, &(glInitProg), "Initprogramm der Agenten", NULL },
		{ "glAgSignatur", 0, 0, G_OPTION_ARG_STRING, &(glAgSignatur), "Signatur der Agenten", "str" },
		{ "glMaxRad", 0, 0, G_OPTION_ARG_INT, &(glMaxRad), "Schritte, die ein Agent pro Laufbefehl zurücklegen kann", NULL },
		{ "glMaxDistTotal", 0, 0, G_OPTION_ARG_INT, &(glMaxDistTotal), "Schritte, die ein Agent maximal pro Schritt (pro Aufruf) zurücklegen kann", NULL },
		{ "glMinNahrung", 0, 0, G_OPTION_ARG_INT, &(glMinNahrung), "Wenn Nahrung kleiner als dieser Wert, sterben", NULL },
		{ "glZLfNr", 0, 0, G_OPTION_ARG_INT, &glZLfNr, "Laufnummer des ersten Agentens", NULL },
		{ "glKProgz", 0, 0, G_OPTION_ARG_DOUBLE, &(glKProgz), "Kosten für eine Programmzeile", "E,zht..." },
		{ "glKz", 0, 0, G_OPTION_ARG_DOUBLE, &(glKz), "Kosten für Zellteilung", "E,zht" },
		{ "glKr", 0, 0, G_OPTION_ARG_DOUBLE, &(glKr), "Kosten für Rennen", "E,zht" },
		{ "glKl", 0, 0, G_OPTION_ARG_DOUBLE, &(glKl), "Kosten für Lesen", "E,zht" },
		{ "glKmin", 0, 0, G_OPTION_ARG_DOUBLE, &(glKmin), "Mindestkosten bei Programmstart zu zahlen", "E,zht" },
		{ "glMaxSchritte", 0, 0, G_OPTION_ARG_INT, &(glMaxSchritte), "Anzahl Schritte laufen, dann Ende", "Zahl" },
		{ "readOpts", 0, 0, G_OPTION_ARG_CALLBACK, readOpts, "lese Optionen aus Datei (geht noch nicht)", NULL },
		{ "saveOpts", 0, 0, G_OPTION_FLAG_OPTIONAL_ARG, saveOpts, "schreibe Optionen in Datei (geht noch nicht)", "dateiname"},
		{ "print", 0, 0, G_OPTION_ARG_CALLBACK, &clineOther, "AlltimeBestAgentsFile auslesen", NULL },
		{ "glExeBest", 0, 0, G_OPTION_ARG_CALLBACK, &clineOther, "AlltimeBestAgentsFile auslesen", NULL },
		{ "glExeBFirstDate", 0, 0, G_OPTION_ARG_CALLBACK, &clineOther, "AlltimeBestAgentsFile auslesen", NULL },
		{ "glSafeBestAt", 0, 0, G_OPTION_ARG_INT, &glSafeBestAt, "besten Agenten sichern nach so vielen Schritten", NULL },
		{ "glAlltimeBestAgentsFile", 0, 0, G_OPTION_ARG_FILENAME, &glAlltimeBestAgentsFile, "Datei, in die regelmäßig die besten Agenten gesichert werden", NULL },
		{ "glDoPaintjob", 0, 0, G_OPTION_ARG_INT, &glDoPaintjob, "soll Grafik ausgegeben werden", NULL},
		{ NULL }
	};
	char **argv=g_strdupv(argvo);
	static GOptionContext *context;
	static bool firstrun=true;
	if (firstrun) {
		context = g_option_context_new ("- Starte Evolutionssimulation");
		g_option_context_add_main_entries (context, entries, NULL);
		g_option_context_add_group (context, gtk_get_option_group (TRUE));
		firstrun=false;
	}
	GError *error = NULL;
	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_print ("option parsing failed: %s\n", error->message);
		exit (1);
	}
	if (strlen(glInfileName)!=0 && strlen(glOutfileName)==0 ) glOutfileName=g_memdup(glInfileName, strlen(glInfileName));
	if (strcmp(glOutfileName, "")==0) glOutfileName=g_memdup(glArenadatei, strlen(glArenadatei)+1);
	printf("glInfileName=%s\n",glInfileName);
	printf("glOutfileName=%s\n",glOutfileName);
}

gint ptlagsortcfunc(GList *a, GList *b) {
	void *vala, *valb;
	vala=((ag*)(a->data))->ptworld;
	valb=((ag*)(b->data))->ptworld;
	if (vala<valb) return -1;
	if (vala==valb) return 0;
	return 1;
}

void distributeAgents(CallbackObject *ptobj, bool verbose) {
//verteile Agenten neu auf die Threads
//verbose schaltet Ausgabe in stdout
	int anzahlAgsInThread=1+ptobj->anz/glNumThreads;
	int z=0;
	if (glNumThreads<2) {
		printf("distributeAgents: glNumThreads==1. Funzt wahrscheinlich noch nicht. Abgeschaltet\n");
		exit (0);
	}
	//ersten Agenten finden im ersten thread, der noch Agenten hat
	GList *firstofallags=0;
	int thread=0;
	while (thread<glNumThreads) {
		if (glptevthreads[thread]->ptfirstag) {
			firstofallags=glptevthreads[thread]->ptfirstag;
			break;
		}
		thread++;
	}
	if (!firstofallags) {
		printf("distributeAgents: Ausgestorben! \n");
		//exit (0);
	}
	GList *ptfirstag=firstofallags;
	//im Falle, dass Listen bereits getrennt sind (immer, außer bei Programmstart), für die folgende Aktion wieder zusammenführen
	if (glNumThreads>1 && ptobj->schritte) { 
		while (thread<glNumThreads-1) {
			GList *ptlastag=glptevthreads[thread]->ptfirstag;
			//suche letzten Agenten eines Threads
			while (ptlastag && ptlastag->next) ptlastag=ptlastag->next;
			//finde ersten Agenten des nächsten Threads (der selbst ebenfalls mindestens einen Agenten enthält)
			ptfirstag=NULL;
			while (thread < glNumThreads-1) {
				thread++;
				if (glptevthreads[thread]->ptfirstag){
					ptfirstag=glptevthreads[thread]->ptfirstag;
					break;
				}
			}
			//verbinde letzten gefundenen Agenten mit erstem gefundenem Agenten des nächsten Threads
			if (ptlastag&&ptfirstag) {
				ptfirstag->prev=ptlastag;
				ptlastag->next=ptfirstag;
				ptfirstag=ptlastag;
			}
		}
	}
	else { //keine Schritte, also Programmanfang
		firstofallags=glptevthreads[0]->ptfirstag;
		printf("setze firstofall %p\n", firstofallags);
	}
	//Alle ptfirstags neu setzen, um Agenten gleichmäßig auf threads zu verteilen
	//-alle firstags auf NULL setzen
	for (int i=0; i<glNumThreads; i++) {
		glptevthreads[i]->ptfirstag=NULL;
	}
	//-extras--versuchen wirs mal mit Sortieren
	firstofallags=g_list_sort(firstofallags, (GCompareFunc) ptlagsortcfunc);
	//printf("bla\n");
	//-ptirstags neu setzen
	GList *ptlag=firstofallags;
	//-bisschen blabla
	if (verbose) {
		printf("   num ags total %i\n", ptobj->anz);
		printf("   num ags per thread %i\n", anzahlAgsInThread);
	}
	for (int thread=0; thread<glNumThreads; thread++) {
		//ptfirstag des jeweiligen threads neu setzen
		glptevthreads[thread]->ptfirstag=ptlag;
		if (verbose) {
			printf("   set firstag of thread No %i to No %i %p\n", thread, z, ptlag);
		}
		if (thread>=glNumThreads-1) goto donerediags;
		//durch die Agenten weiterzählen
		for (int i=0; i<anzahlAgsInThread; i++) {
			if (ptlag && ptlag->next) ptlag=ptlag->next;
			else {
				printf("t %i zu wenig Agenten in ->ptfirstag %i\n", thread, i);
				goto donerediags;
			}
			z++;
		}
	}
	donerediags:
	//die zusammenhängenden Enden der Liste kappen
	for (int thread=0; thread<glNumThreads-1; thread++) {
		if (glptevthreads[thread+1]->ptfirstag) {
			glptevthreads[thread+1]->ptfirstag->prev->next=NULL;
			glptevthreads[thread+1]->ptfirstag->prev=NULL;
		}
	}
	if (verbose) printf("   done distributeAgents to threads\n");
	//ptnth->ptfirstag finden
}

void sterben(GList *ptlag, tevthreads* ptthstruct) {
//ptlag stirbt. Nutri verbleiben, wenn noch vorhanden, in der Matrix
	g_mutex_lock(&glmZellte);
	if (((ag*)(ptlag->data))->agnr==glfollowedAg) {
		glfollowedAg=1;
		printf("\nAgent wird abgeschaltet\n");
	}
	CallbackObject *ptobj=ptthstruct->ptobj;
	ptobj->anz--;
	//Werte sichern
	int num=getAgind(ptlag->data);
	//paintjob
	if (ptobj->doPaintjob) {
		int x=getXFromWpt(((ag*)(ptlag->data))->ptworld);
		int y=getYFromWpt(((ag*)(ptlag->data))->ptworld);
		if (pixisvis(x,y,ptobj)) {
			tpaintlist plistel;
			plistel.x=(x-ptobj->xorig+glWorldsize)%glWorldsize;
			plistel.y=(y-ptobj->yorig+glWorldsize)%glWorldsize;
			plistel.type=DLCLTDIE;
			plistel.count=0;
			appendToPlistOnce(&plistel, ptthstruct->ptobj);
		}
	}
	//matrix pflegen
	if (((ag*)(ptlag->data))->ptworld) ((ag*)(ptlag->data))->ptworld->Agind=0; 
	//Leiche herrichten (recyclen), ab in den Orkus
	g_free(((ag*)(ptlag->data))->mem);
	//den Agenten ausklinken
	ptthstruct->ptfirstag=g_list_delete_link(ptthstruct->ptfirstag,ptlag);
	//distri korrigieren
	ptdistri=g_slist_prepend(ptdistri, GINT_TO_POINTER(num));
	//etc
	g_mutex_unlock(&glmZellte);
}

void evschrittth (tevthreads *ptthstruct) {
//Unternehme einen Schritt. Minimal threadsafe
//dh. gehe durch alle Agenten und Prüfe auf Lebensstatus, führe Programme der Agenten aus etc
//große Schleife. Thread soll "ewig" laufen
	static int threadsdone=0;
	bool operate=true;
	while (operate) {
		//threadspezifische Anfangsarbeiten 
		g_mutex_lock(&ptthstruct->mutex);
		//Anfangsarbeiten
		GList *ptlag=ptthstruct->ptfirstag;
		//große Programmschleife für Agenten
		int z=0;
		while (ptlag) {
			//überprüfe auf Lebensstatus, beerdige tote Agenten
			if (ptlag->data==NULL) printf("ptlag->data==NULL t %i z  %i ptlag %p\n", ptthstruct->threadnum, z, ptlag);
			while (ptlag!=NULL && \
					(((ag*)(ptlag->data))->ptworld==NULL || \
					((ag*)(ptlag->data))->ptworld->Nutri==0 || \
					((ag*)(ptlag->data))->sterben!=0)) {
				GList *ptraus=ptlag;
				ptlag=ptlag->next;
				z++;
				sterben(ptraus, ptthstruct);
			}
			//rufe Agenten auf und führe Programme aus
			if (ptlag) {
				z++;
				runprog(ptlag->data, ptthstruct);
				ptlag=ptlag->next;
			}
		}
		//Zusatzarbeiten pro Schritt
		//regnenpth(ptthstruct->ptobj); 
		//-Hauptthread wieder starten
		g_mutex_lock(&glmttthdone);
		threadsdone++;
		//if (threadsdone==1) regnen(ptthstruct->ptobj);
		if (threadsdone==glNumThreads) {
			g_mutex_unlock(&glmsced);
			threadsdone=0;
		}
		g_mutex_unlock(&glmttthdone);
		ptthstruct->schritt++;
	}
	printf("thread %i endet\n", ptthstruct->threadnum);
}

void evthreadsinit(CallbackObject *ptobj) {
//die Threads (Anzahl=glNumThreads) für evschrittth starten und initialisieren
	//blabla
	printf("Set up threads.\n   Anzahl Threads=%i\n", glNumThreads);
	glptevthreads=g_new(tevthreads*, glNumThreads);
	for (int i=0; i<glNumThreads; i++) {
		tevthreads *ptnth=g_new(tevthreads,1);
		if (!ptnth) {
			printf("evinit: kein ptnth\n");
			exit (0);
		}
		glptevthreads[i]=ptnth;
		ptnth->ptfirstag=NULL;
		ptnth->threadnum=i;
		ptnth->schritt=0;
		ptnth->ptobj=ptobj;
		g_mutex_init(&ptnth->mutex);
		g_mutex_lock(&ptnth->mutex);
		ptnth->addtomatrix=0;
		ptnth->ckosten=0;
	}
	if (!glptevthreads) {
		printf("glptevthreads init hat nicht geklappt. Tschüss\n");
		exit (0);
	}
	glptevthreads[0]->ptfirstag=glptagFromInitAgs;
	distributeAgents(ptobj, 1);
	//Threads starten
	for (int i=0; i<glNumThreads; i++) {
		g_thread_new(0, (GThreadFunc) evschrittth, glptevthreads[i]);
	}
	printf("done set up threads.\n");
}

void evinitparse(CallbackObject *ptobj, int argc, char **argv){
//die von der commandline abhängigen Aufgaben von evinit ausführen
	void delBestAgList() {
		return;
		printf("lösche liste mit %i Einträgen\n", g_list_length(glBestAgs));
		GList *ptl=glBestAgs;
		while (ptl) {
			g_free(((ag*)(ptl->data))->mem);
			ptdistri=g_slist_prepend(ptdistri,GINT_TO_POINTER(getAgind(ptl->data)));
			ptl=ptl->next;
		}
		printf("      ... not done\n");
	}
//Anfang	
	printf("parse commandline\n");
	//Commandline auswerten
	parseCommandline(argc, argv);
	if (!glDoPaintjob) {
		gtk_widget_hide(ptobj->ptmalfenster1);
		gtk_widget_hide(ptobj->ptmalfenster2);
		ptobj->doPaintjob=FALSE;
		
	}
	//Vorbereitungen für Programmstart durchführen
	//-Teste Datei mit besten Agenten auf ausführbarkeit und sichere ausführbare Agenten in neuer Datei
	if (glTestlaufBestAgs) {
		printf("Option glTestlaufBestAgs ausgeschaltet\n");
		exit (0);
		initWorld();
		leseAlltimeBestAgentsFile(ptobj, 4);
		initAgs(ptobj, 1);
	}
	//-normaler Lauf mit besten Agenten aus entsprechendem File
	//der Wert von glExeBest ist die Anzahl der letzten Agenten aus diesem file
	else if (glExeBest) {
		printf("Starte dlab mit besten Agenten \n");
		printf("   verteile letzte %i Sampleprogs auf Matrix. Anzahl Agenten pro Sample= %i\n", glExeBest, glNumAgents/glExeBest);
		printf("   todo: lösche Liste mit besten Agenten\n");
		initWorld();
		leseAlltimeBestAgentsFile(ptobj, 3);
		initAgs(ptobj, 1);
		printf("    lösche Liste...");
		delBestAgList();
		printf("   ...done.\n");
	}
	else if (strlen(glExeBFirstDate)) { 
		printf("Starte dlab mit besten Agenten ab Datum %s\n", glExeBFirstDate);
		initWorld();
		leseAlltimeBestAgentsFile(ptobj, 3); //hier wird u. a. glExeBest hochgesetzt
		printf("   verteile letzte %i Sampleprogs auf Matrix. \n      Anzahl Agenten pro Sample= %f\n", glExeBest, (float)glNumAgents/glExeBest);
		printf("   todo: lösche Liste mit besten Agenten\n");
		initAgs(ptobj, 1);
		printf("    lösche Liste... nicht");
		delBestAgList();
		printf("   ...done.\n");
	}
	//-normalfall, keine besonderen switches gesetzt
	else if (strlen(glInfileName)==0) {
		initWorld();
		initAgs(ptobj, 1);
	}
	if (ptobj->doPaintjob)
		gtk_toggle_button_set_active((GtkToggleButton *)gtk_builder_get_object(ptobj->builder, "arenazeigen"), true);
	else
		gtk_toggle_button_set_active((GtkToggleButton *)gtk_builder_get_object(ptobj->builder, "arenazeigen"), false);
}

void initAgArr() {
	for (int i=0; i<(1<<16); i++) {
		agArr[i].Signatur=g_new(char, glSigsize);
	}
}
void evimain(CallbackObject *ptobj) {
//inits übernommen aus main
	GError *error = NULL;
	GtkWidget *window, *arena, *grwindow;
	ptobj->builder = gtk_builder_new ();
	//gtk_builder_add_from_file (ptobj->builder, "/home/xharx/Dropbox/Programmieren/evsim/evsimiii/v57.glade", &error);
	gtk_builder_add_from_file (ptobj->builder, "v57.glade", &error);
	window = GTK_WIDGET(gtk_builder_get_object (ptobj->builder, "mainwindow"));
	grwindow = GTK_WIDGET(gtk_builder_get_object (ptobj->builder, "grarenawindow"));
	arena = GTK_WIDGET(gtk_builder_get_object (ptobj->builder, "arena"));
	ptobj->ptmalfenster1=arena;
	ptobj->ptmalfenster2=grwindow;
	gtk_widget_hide(ptobj->ptmalfenster2);
	ptobj->ptstatsfenster= GTK_WIDGET(gtk_builder_get_object (ptobj->builder, "Statswindow"));
	gtk_builder_connect_signals (ptobj->builder, ptobj);
	g_signal_connect (window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	gtk_widget_show_all (window);
	//gtk_widget_show_all (ptobj->ptmalfenster2);
	//Fenstersystem
	ptobj->ptmalfenster=ptobj->ptmalfenster1;
	//GdkScreen *ptscreen=gdk_screen_get_default();
	GdkDisplay *ptdisplay=gdk_display_get_default();
	GdkMonitor *ptmonitor=gdk_display_get_primary_monitor(ptdisplay);
	GdkRectangle geo;
	gdk_monitor_get_geometry(ptmonitor, &geo);
	printf("monitor %i %i %i %i\n", geo.x, geo.y, geo.width, geo.height);
	ptobj->monitorgeox=geo.width;
	ptobj->monitorgeoy=geo.height;
}

void evinit(CallbackObject *ptobj, int argc, char **argv){
//alle Vorarbeiten vor Aufruf der großen Schleife
	printf("evinit\n");
	printf("DLBEFLAST=%i\n", DLBEFLAST);
	evimain(ptobj);
	writeLookups();
	initAgArr();
	glMatrixpoints=0;
	glptGrand=g_rand_new();
	initdistro(); 
	g_mutex_init(&glmZellte);
	g_mutex_init(&glmFangen);
	g_mutex_init(&glmATP);
	g_mutex_init(&glmsced);
	//noch ein paar inits
	evinitparse(ptobj, argc, argv); //inits von der Kommandozeile
	evthreadsinit(ptobj); //init Threadsystem der Agenten
	initFlood(ptobj); //Ursprungsberegnung
	printf("done evinit.\n");
}

void malePunktn(tpaintlist* ptp, CallbackObject *ptobj) {
//einen Pixel malen
	void copyColordataToDaten(color *ptcolor, guchar *ptdaten, float hue) {
		float helligkeit=ptcolor->red+ptcolor->green+ptcolor->blue;
		float mindesthelligkeit=200;
		float faktor=1;
		if (helligkeit<mindesthelligkeit) faktor=mindesthelligkeit/helligkeit;
		ptdaten[0]=ptcolor->red*faktor*hue;
		ptdaten[1]=ptcolor->green*faktor*hue;
		ptdaten[2]=ptcolor->blue*faktor*hue;
	}

	//printf("daten %p adr%p x %i y %i c %i\n", ptobj->daten, &ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+0], ptp->x, ptp->y, ptp->type);
	if (!ptp) return;
	//Plausibilitätsprobe
	if (ptp->x>ptobj->breite || ptp->y>ptobj->hoehe) {
		printf("count %i länge %i\n", ptp->count, g_slist_length(ptobj->ptplist));
		printf("strange in malePunkt %i %i\n", ptp->x, ptp->y);
		GSList *ptol=ptobj->ptplist;
		int i=0;
		while (ptol) {
			printf("i %i x %i y %i c %i\n" \
				, i, ((tpaintlist *)(ptol->data))->x, ((tpaintlist *)(ptol->data))->y, ((tpaintlist *)(ptol->data))->count);
			printf("colors %i %i %i\n", \
				((tpaintlist *)(ptol))->color.red, ((tpaintlist *)(ptol))->color.green, ((tpaintlist *)(ptol))->color.blue);
			i++;
			ptol=ptol->next;
		}
		exit (0);
		return;
	}
	if (ptp->type==DLCLTNULL) {
		if ((world+((glWorldsize+ptp->y+ptobj->yorig)%glWorldsize)*glWorldsize+(glWorldsize+ptp->x+ptobj->xorig)%glWorldsize)->Nutri==0) {
			ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+0]=nullcolor.red;
			ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+1]=nullcolor.green;
			ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+2]=nullcolor.blue;
		}
		else {
			ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+0]=raincolor.red;
			ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+1]=raincolor.green;
			ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+2]=raincolor.blue;
		}
	}
	else if (ptp->type==DLCLTAG) {
		copyColordataToDaten( &ptp->color, (guchar *)&(ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+0]), 1);
		/*
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+0]=ptp->color.red;
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+1]=ptp->color.green;
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+2]=ptp->color.blue;
		*/
		reappendToPlist(30, ptp, ptobj);
	}
	else if (ptp->type==DLCLTSCHLEIMSPUR) {
		int appcnt=ptp->Nutri/4;
		color *ccolor=g_memdup(&ptp->scolor,sizeof(color));
		float hue=(float)(1-(float)(ptp->count-1)/appcnt);
		copyColordataToDaten( ccolor, (guchar *)&(ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+0]), hue);
		/*
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+0]=(unsigned char)((float)ccolor->red*(float)(1-(float)(ptp->count-1)/appcnt));
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+1]=(unsigned char)((float)ccolor->green*(float)(1-(float)(ptp->count-1)/appcnt));
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+2]=(unsigned char)((float)ccolor->blue*(float)(1-(float)(ptp->count-1)/appcnt));
		*/
		g_free(ccolor);
		reappendToPlist(appcnt, ptp, ptobj);
	}
	else if (ptp->type==DLCLTDIE) {
		int appcnt=100;
		//ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+0]=diecolor.red;
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+0]=(unsigned char)((float)diecolor.red*(float)(1-(float)(ptp->count-1)/appcnt));
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+1]=(unsigned char)((float)diecolor.green*(float)(1-(float)(ptp->count-1)/appcnt));
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+2]=(unsigned char)((float)diecolor.blue*(float)(1-(float)(ptp->count-1)/appcnt));
		reappendToPlist(appcnt, ptp, ptobj);
	}
	else if (ptp->type==DLCLTRAIN) { //wird nicht wieder an ptp angefügt
		//Wert wird neu geschrieben durch initarena
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+0]=raincolor.red;
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+1]=raincolor.green;
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+2]=raincolor.blue;
	}
	else if (ptp->type==DLCLTAGFUG) {
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+0]=agcolorfug.red;
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+1]=agcolorfug.green;
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+2]=agcolorfug.blue;
		reappendToPlist(30, ptp, ptobj);
	}
	else if (ptp->type==DLCLTTEST) {
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+0]=testcolor.red;
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+1]=testcolor.green;
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+2]=testcolor.blue;
	}
	else if (ptp->type==DLCLTGEFRESSEN) {
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+0]=gefressencolor.red;
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+1]=gefressencolor.green;
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+2]=gefressencolor.blue;
		reappendToPlist(100, ptp, ptobj);
	}
	else if (ptp->type==DLCLTLESEN) {
		//copyColordataToDaten( & ptp->color, (guchar *)&(ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+0]));
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+0]=ptp->color.red;
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+1]=ptp->color.green;
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+2]=ptp->color.blue;
		//ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+0]=lesencolor.red;
		//ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+1]=lesencolor.green;
		//ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+2]=lesencolor.blue;
		reappendToPlist(4, ptp, ptobj);
	}
	else if (ptp->type==DLCLTLESENLONGDIST) {
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+0]=ptp->color.red;
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+1]=ptp->color.green;
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+2]=ptp->color.blue;
		//ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+0]=lesenlongdistcolor.red;
		//ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+1]=lesenlongdistcolor.green;
		//ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+2]=lesenlongdistcolor.blue;
		reappendToPlist(4, ptp, ptobj);
	}
	else if (ptp->type==DLCLTLESENFOUND) {
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+0]=lesenfoundcolor.red;
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+1]=lesenfoundcolor.green;
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+2]=lesenfoundcolor.blue;
		reappendToPlist(30, ptp, ptobj);
	}
	else if (ptp->type==DLCLTLESENFOUNDHIMSELF) {
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+0]=lesenfoundhimselfcolor.red;
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+1]=lesenfoundhimselfcolor.green;
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+2]=lesenfoundhimselfcolor.blue;
		reappendToPlist(10, ptp, ptobj);
	}
	else if (ptp->type==DLCLTLESENFOUNDNOTTAKEN) {
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+0]=lesenfoundnottakencolor.red;
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+1]=lesenfoundnottakencolor.green;
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+2]=lesenfoundnottakencolor.blue;
		reappendToPlist(60, ptp, ptobj);
	}
	else if (ptp->type==DLCLTLESENFOUNDGEFRESSEN) {
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+0]=lesenfoundgefressencolor.red;
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+1]=lesenfoundgefressencolor.green;
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+2]=lesenfoundgefressencolor.blue;
		reappendToPlist(100, ptp, ptobj);
	}
	else if (ptp->type==DLCLTKILLER) {
		//copyColordataToDaten( & ptp->color, (guchar *)&(ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+0]));
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+0]=ptp->color.red;
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+1]=ptp->color.green;
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+2]=ptp->color.blue;
		//ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+0]=killercolor.red;
		//ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+1]=killercolor.green;
		//ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+2]=killercolor.blue;
		reappendToPlist(30, ptp, ptobj);
	}
	else if (ptp->type==DLCLTRULER) {
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+0]=rulercolor.red;
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+1]=rulercolor.green;
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+2]=rulercolor.blue;
		reappendToPlist(0, ptp, ptobj);
	}
	else if (ptp->type==DLCLTREADHS) {
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+0]=readhscolor.red;
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+1]=readhscolor.green;
		ptobj->daten[ptp->y*ptobj->breite*3+ptp->x*3+2]=readhscolor.blue;
		reappendToPlist(30, ptp, ptobj);
	}
}

void arbeitePaintlistAb(CallbackObject *ptobj) {
//alle Punkte in der Liste zu malender Pixel abarbeiten
	GSList *ptp=ptobj->ptplist;
	//liste durchgehen und Punkte abarbeiten
	while (ptp) {
		malePunktn(ptp->data, ptobj);
		ptp=ptp->next;
	}
	//alte Liste deallozieren
	g_slist_foreach(ptobj->ptplist,(GFunc)g_free,NULL);
	g_slist_free(ptobj->ptplist);
	//neue Liste zur aktuellen Liste machen
	ptobj->ptplist=ptobj->ptnplist;
	//neue Liste null setzen
	ptobj->ptnplist=NULL;
}

void paintjob(CallbackObject *ptobj) {
//Alle Arbeiten ausführen, um Matrix und Agenten auf den Bildschirm zu malen
	if (glTriggerInitArena==1) {
		glTriggerInitArena=0;
		initarena(ptobj);
	}
	if (ptobj->doPaintjob) {
		arbeitePaintlistAb(ptobj);
		gtk_widget_queue_draw(ptobj->ptmalfenster);
	}
}

void makeDatumString(char *ptbak) {
//einen String aus Ziffern zurückgeben mit je zwei Stellen für Jahr, Monat, Tag, Stunde, Sekunde des Aufrufzeitpunkts (Systemzeit)
	struct tm *ptmtm;
	time_t tnow;
	time(&tnow);
	ptmtm=localtime(&tnow);
	sprintf(ptbak,"%02i%02i%02i%02i%02i%02i",ptmtm->tm_year-100, ptmtm->tm_mon+1, ptmtm->tm_mday, ptmtm->tm_hour, ptmtm->tm_min, ptmtm->tm_sec);
}

GList* getnextag(GList *ptlag) {
//nicht threadsafe
//liefere nächsten Agenten, wenn nötig vom nächsthöheren thread, wenn letzter Agent erreicht, NULL
	static int clist=0;
	ptlag=ptlag->next;
	while (!ptlag && clist<glNumThreads-1) {
		clist++;
		ptlag=glptevthreads[clist]->ptfirstag;
	}
	if (!ptlag) {
		clist=0;
	}
	return ptlag;
}

GList *getfirstag() {
//nicht threadsafe
//liefere zuverlässig den ersten Agenten im ersten thread, der noch Agenten führt
	for (int i=0; i<glNumThreads; i++) {
		if (glptevthreads[i]->ptfirstag) return glptevthreads[i]->ptfirstag;
	}
	return NULL; //keine Agenten vorhanden
}

char* getStringOfSignatures() {
//alle aktuell laufenden Signaturen mit Anzahl der Agenten ausgeben. Beste Signatur markieren.
	long int bestval=0;
	char *raus=malloc(2);
	memcpy(raus,"\0",1);
	//char *bestsig=0;
	void hauswerten(gpointer key, gpointer val, gpointer nix) {
		char besttag; //Das Zeichen, das die beste Signatur markieren soll
		if ((long)val>bestval) {
			bestval=(long)val;
			//bestsig=key;
			besttag='*';
		}
		else besttag=' ';
		int strln;
		if (raus) strln=strlen(raus);
		else strln=0;
		raus=realloc(raus,strln+strlen(key)+12+2+2);
		if (!raus) {
			printf("findbestsig: kein Speicher\n");
			exit (0);
		}
		sprintf(raus, "%s %s, %li %c\n", raus, (char *) key, (long int) val, besttag);
	}
//Anfang
	GList *ptag=getfirstag();
	GHashTable *ghsigs=g_hash_table_new(g_str_hash, g_str_equal);
	while (ptag) {
		if (((ag*)(ptag->data))->sterben) {
			ptag=getnextag(ptag);
			continue;
		}
		//Signaturen zählen
		g_hash_table_insert(ghsigs, ((ag *)(ptag->data))->Signatur, g_hash_table_lookup(ghsigs, ((ag *)(ptag->data))->Signatur)+1);
		ptag=getnextag(ptag);
	}
	//gebe Liste mit Signaturen und Anzahl der Agenten aus, markiere ermittelte beste Signatur
	g_hash_table_foreach(ghsigs, hauswerten, 0);
	return raus;
}

ag *findBestAgent() {
//finde den besten Agenten der gegenwärtigen Bevölkerung. nicht threadsafe
//Variablen	
	ag * ptbestag=NULL;
	int bestpoints=0; //irreführend: numNachfahren eines Agenten
	long int bestval=0;//Anzahl der Agenten mit einer bestimmten Signatur
	char *bestsig=0; //Die Signatur mit den meisten Agenten
//Routinen	
	void considerthisone(gpointer ptin, gpointer nix) {
		ag *ptag=(ag *)ptin;
		if (!strcmp(ptag->Signatur, bestsig)) {
			if (ptag->numNachfahren>bestpoints) {
				bestpoints=ptag->numNachfahren;
				ptbestag=ptag;
			}
		}
	}
	void hauswerten(gpointer key, gpointer val, gpointer nix) {
		char besttag;
		if ((long)val>bestval) {
			bestval=(long)val;
			bestsig=key;
			besttag='*';
		}
		else besttag=' ';
		printf("%s, %li %c\n", (char *) key, (long int) val, besttag);
	}
	void findbestsig() {
	//finde beste Signatur und gebe Liste aller Signaturen aus
		GList *ptag=getfirstag();
		GHashTable *ghsigs=g_hash_table_new(g_str_hash, g_str_equal);
		while (ptag) {
			if (((ag*)(ptag->data))->sterben) {
				ptag=getnextag(ptag);
				continue;
			}
			//Signaturen zählen
			g_hash_table_insert(ghsigs, ((ag *)(ptag->data))->Signatur, g_hash_table_lookup(ghsigs, ((ag *)(ptag->data))->Signatur)+1);
			ptag=getnextag(ptag);
		}
		//gebe Liste mit Signaturen und Anzahl der Agenten aus, markiere ermittelte beste Signatur
		g_hash_table_foreach(ghsigs, hauswerten, 0);
	}
//Anfang
	printf("Signaturen, Agenten, letzter'*'=bester\n");
	findbestsig();
	for (int i=0; i<glNumThreads; i++) {
		g_list_foreach(glptevthreads[i]->ptfirstag, considerthisone, 0);
	}
	printf("bestpoints=%i\n", bestpoints);
	if (ptbestag) glfollowedAg=ptbestag->agnr;
	return ptbestag;
}

bool saveBestAgent() {
//finde besten Agenten und schreibe Ergebnis in Datei
	ag *ptbest=findBestAgent();
	printf("saveBestAgent done findBestAgent \n");
	if (ptbest==0) {
		printf("kein bestAgent\n");
		return false;
	}
	//kopf kreieren
	tbestAgentKopf bestAgentKopf;
	makeDatumString(bestAgentKopf.datum);
	bestAgentKopf.myversion=4;
	bestAgentKopf.memsize=ptbest->memsize;
	strcpy(bestAgentKopf.signatur,glAgSignatur);
	strcat(bestAgentKopf.signatur,"$d");
	strcat(bestAgentKopf.signatur,bestAgentKopf.datum);
	//-Farben zum Sichern kopieren
	bestAgentKopf.bocolor=ptbest->bocolor;
	bestAgentKopf.iycolor=ptbest->iycolor;
	bestAgentKopf.iyfcolor=ptbest->iyfcolor;
	bestAgentKopf.bofcolor=ptbest->bofcolor;
	bestAgentKopf.schleimspur=ptbest->schleimspur;
	//in Datei schreiben
	FILE *datei=g_fopen(glAlltimeBestAgentsFile, "a+");
	int sizeofkopf=sizeof(bestAgentKopf);
	fwrite(&sizeofkopf, sizeof(int), 1, datei);
	fwrite(&bestAgentKopf, sizeof(bestAgentKopf), 1, datei);
	//Größe des ->mem in Datei schreiben
	fwrite(&ptbest->memsize, sizeof(int), 1, datei);
	//mem schreiben
	fwrite(ptbest->mem, ptbest->memsize, 1, datei);
	fclose(datei);
	//Infodatei schreiben
	char *glInfodatei="dlabinfo.txt";
	datei=g_fopen(glInfodatei, "a+");
	char *signatures=getStringOfSignatures();

	fwrite(signatures, strlen(signatures), 1, datei);
	free(signatures);
	fwrite(ptbest->Signatur, strlen(ptbest->Signatur), 1, datei);
	fwrite(" ", 1, 1, datei);
	fwrite(&bestAgentKopf.datum, strlen(bestAgentKopf.datum), 1, datei);
	fwrite("\n\n", 1, 1, datei);
	fclose(datei);
	//blabla
	printf("best Agent orig Signatur=%s, neue Signatur %s\n", ptbest->Signatur, bestAgentKopf.datum);
	return true;
}

char *makePrefsAnzeige1() {
	GString *ptraus=g_string_new("Preferences:\n");
	g_string_append_printf(ptraus,"glNumThreads %i\n", glNumThreads);
	g_string_append_printf(ptraus,"glWorldsize %i\n", glWorldsize);
	g_string_append_printf(ptraus,"glNumRegen %i\n", glNumRegen);
	g_string_append_printf(ptraus,"glValRegen %i\n", glValRegen);
	g_string_append_printf(ptraus,"glMaxRad %i\n", glMaxRad);
	g_string_append_printf(ptraus,"glMaxDistTotal %i\n", glMaxDistTotal);
	g_string_append_printf(ptraus,"glABAFile %s\n", glAlltimeBestAgentsFile);
	return ptraus->str;
}

char *makePrefsAnzeige2() {
	GString *ptraus=g_string_new("Kosten:\n");
	g_string_append_printf(ptraus,"glKProgz %f\n", glKProgz);
	g_string_append_printf(ptraus,"glKl %f\n", glKl);
	g_string_append_printf(ptraus,"glKr %f\n", glKr);
	g_string_append_printf(ptraus,"glKz %f\n", glKz);
	g_string_append_printf(ptraus,"glKcpline %f\n", glKcpline);
	g_string_append_printf(ptraus,"glKmin %f\n", glKmin);
	return ptraus->str;
}

char *makePrefsAnzeigeMut() {
	GString *ptraus=g_string_new("mutabor:\n");
	g_string_append_printf(ptraus,"glmutpc %i\n", glmutpc);
	g_string_append_printf(ptraus,"glmutinsline %i\n", glmutinsline);
	g_string_append_printf(ptraus,"glmutdelline %i\n", glmutdelline);
	g_string_append_printf(ptraus,"glmutmodline %i\n", glmutmodline);
	g_string_append_printf(ptraus,"glmutmodlineprob %i\n", glmutmodlineprob);
	g_string_append_printf(ptraus,"glmutbits %i\n", glmutbits);
	return ptraus->str;
}

int stats(CallbackObject *ptobj) {
//alle Statistikdaten sammeln und auf den Bildschirm schreiben
	char *addtostatstext(char *statstext, char *puf) {
		char *raus=realloc(statstext, strlen(statstext)+strlen(puf)+1);
		if (!raus) exit (0);
		raus=strcat(raus, puf);
		return raus;
	}
	char *addlinetst(char *in, char *name, char *numbers) {
		char *out=addtostatstext(in, name);
		out=addtostatstext(out, "  ");
		out=addtostatstext(out, numbers);
		out=addtostatstext(out, "\n");
		return out;
	}
//Anfang
	char *statstext=malloc(1);
	statstext[0]=0x00;
	char puf[100];
	sprintf(puf,"%lu",ptobj->schritte);
	statstext=addlinetst(statstext,"Schritte", puf);
	sprintf(puf,"%lu",glZLfNr);
	statstext=addlinetst(statstext, "Laufnummer", puf);
	sprintf(puf,"%i",ptobj->anz);
	statstext=addlinetst(statstext, "Anzahl", puf);
	//Geburten/Schritt
	sprintf(puf,"%f",(float)glZLfNr/ptobj->schritte);
	statstext=addlinetst(statstext, "Geburten/Schritt", puf);
	//dichte
	g_mutex_lock(&glmsced);
	for (int i=0; i<glNumThreads; i++) {
		glMatrixpoints+=glptevthreads[i]->addtomatrix;
		glptevthreads[i]->addtomatrix=0;
	}
	g_mutex_unlock(&glmsced);
	sprintf(puf,"%f",(float)glMatrixpoints/(glWorldsize*glWorldsize));
	statstext=addlinetst(statstext, "Dichte", puf);
	//statstext=addlinetst(statstext, "Dichte", puf);
	//Agenten pro Sekunde
	static gint64 lastt=0;
	static gint lschritte=0;
	gint64 currt=g_get_real_time();
	sprintf(puf,"%lu",(long)ptobj->anz*(ptobj->schritte-lschritte)*1000000/(currt-lastt));
	//gtk_label_set_text((GtkLabel*)gtk_builder_get_object(ptobj->builder, "Agstn"),puf);
	statstext=addlinetst(statstext, "Ags/sec", puf);
	lastt=currt;

	//Ausgabe Anzahl Nahrung (AnzNahrung)
	//errechne Durchschnitt Proglänge, sumNutriInAgents
	GList *ptag=getfirstag();
	gint zl=0;
	int sumNutriInAgents=0;
	int genHi=0;
	int genHiMut=0;
	int linesdone=0;
	long unsigned int allage=0;
	GHashTable *ghsigs=g_hash_table_new(g_str_hash, g_str_equal);
	while (ptag) {
		zl++;
		if (((ag*)(ptag->data))->sterben) {
			ptag=getnextag(ptag);
			continue;
		}
		//if (!ptag) continue; 
		if (((ag*)(ptag->data))->ptworld)
			sumNutriInAgents+=((ag*)(ptag->data))->ptworld->Nutri;
		else {
			printf("stats: kein ->ptworld %i\n", zl);
		}
		if (((ag*)(ptag->data))->generationenZ>genHi) genHi=((ag*)(ptag->data))->generationenZ;
		if (((ag*)(ptag->data))->generationenMutZ>genHiMut) genHiMut=((ag*)(ptag->data))->generationenMutZ;
		linesdone+=((ag*)(ptag->data))->linesdone;
		allage+=ptobj->schritte-((ag*)(ptag->data))->geburt;
		//Signaturen zählen
		g_hash_table_insert(ghsigs, ((ag *)(ptag->data))->Signatur, g_hash_table_lookup(ghsigs, ((ag *)(ptag->data))->Signatur)+1);
		ptag=getnextag(ptag);
	}
	//Anzahl der abgearbeiteten Programmzeilen pro Agent
	sprintf(puf,"%f",(float)linesdone/allage);
	statstext=addlinetst(statstext, "Proglines/ag", puf);
	//Anzahl der Points pro Agent
	sprintf(puf,"%f",(float)sumNutriInAgents/zl);
	statstext=addlinetst(statstext, "Nutri/ag", puf);
	//Kosten pro Schritt 
	//###threadsafe machen
	g_mutex_lock(&glmsced);
	int ksum=0;
	for (int i=0; i<glNumThreads; i++) {
		ksum+=glptevthreads[i]->ckosten;
		glptevthreads[i]->ckosten=0;
	}
	g_mutex_unlock(&glmsced);
	sprintf(puf,"%f",(float)ksum/ptobj->anz/(ptobj->schritte-lschritte));
	statstext=addlinetst(statstext, "Kosten/Schritt", puf);
	//höchste Generation
	sprintf(puf,"%i",genHi);
	statstext=addlinetst(statstext, "genHi", puf);
	//höchste mutierte Generation
	sprintf(puf,"%i",genHiMut);
	statstext=addlinetst(statstext, "genHiMut", puf);
	//höchste Generationen / höchste mutierte Generationen
	if (genHiMut) sprintf(puf,"%f",(float)genHi/genHiMut);
	else sprintf(puf,"%s","nd");
	statstext=addlinetst(statstext, "Hi/HiMut", puf);
	//Schritte/Generation
	if (genHiMut) sprintf(puf,"%f",(float)ptobj->schritte/genHiMut);
	else sprintf(puf,"%s","nd");
	statstext=addlinetst(statstext, "Schritte/genHiMut", puf);

	//Anzahl verbleibender Signaturen
	int numsigs=g_hash_table_size(ghsigs);
	g_hash_table_destroy(ghsigs);
	sprintf(puf,"%i", numsigs);
	statstext=addlinetst(statstext, "NumSigs", puf);
	//Selbstmorde
	sprintf(puf,"%i",glSuicideCnt);
	statstext=addlinetst(statstext, "glSuicideCnt", puf);
	//Vergrößerung, Verkleinerung von ->memsize
	sprintf(puf,"%i",glMemInc);
	statstext=addlinetst(statstext, "glMemInc", puf);
	sprintf(puf,"%i",glMemDec);
	statstext=addlinetst(statstext, "glMemDec", puf);
	gtk_label_set_text((GtkLabel*)gtk_builder_get_object(ptobj->builder, "textfeld1"),statstext);
	free(statstext);
	//zum Schluss noch statische Werte neu schreiben
	ptobj->PrefsAnzeige1=makePrefsAnzeige1();
	ptobj->PrefsAnzeige2=makePrefsAnzeige2();
	ptobj->PrefsAnzeige3=makePrefsAnzeigeMut();
	gtk_label_set_text((GtkLabel*)gtk_builder_get_object(ptobj->builder, "prefstext1"),ptobj->PrefsAnzeige1);
	gtk_label_set_text((GtkLabel*)gtk_builder_get_object(ptobj->builder, "prefstext2"),ptobj->PrefsAnzeige2);
	gtk_label_set_text((GtkLabel*)gtk_builder_get_object(ptobj->builder, "prefstext3"),ptobj->PrefsAnzeige3);
	lschritte=ptobj->schritte;

	return 1;
}

CallbackObject *initCallbackObj() {
	CallbackObject *ptobj=g_new(CallbackObject, 1);
	ptobj->daten=NULL;
	ptobj->pixbuf=NULL;
	ptobj->pixbufAnzeige3=NULL;
	ptobj->pixbufAnzeige3=0;//gdk_pixbuf_new_from_file("/tmp/evsim.tmp",0);
	ptobj->breite=800;
	ptobj->hoehe=600;
	ptobj->zeilenlaenge=3*ptobj->breite;
	ptobj->daten=g_new(guchar, ptobj->zeilenlaenge*ptobj->hoehe);
	ptobj->pixbuf=gdk_pixbuf_new_from_data(ptobj->daten, GDK_COLORSPACE_RGB, FALSE, 8, ptobj->breite, ptobj->hoehe, ptobj->zeilenlaenge, NULL, NULL);
	ptobj->schritte=0;
	ptobj->ptplist=NULL;
	ptobj->ptnplist=NULL;
	ptobj->doPaintjob=true;
	ptobj->langsam=0;
	/*
	ptobj->PrefsAnzeige1=makePrefsAnzeige1();
	ptobj->PrefsAnzeige2=makePrefsAnzeige2();
	ptobj->PrefsAnzeige3=makePrefsAnzeigeMut();
	*/
	ptobj->PrefsAnzeige1=0;
	ptobj->PrefsAnzeige2=0;
	ptobj->PrefsAnzeige3=0;
	ptobj->xorig=0;
	ptobj->yorig=0;
	ptobj->anz=0;
	return ptobj;
}

gboolean draw_window (GtkWidget *fenster, cairo_t *cr, CallbackObject *obj) {
//wird von gtk+ aufgerufen, arenafenster
	if (fenster==obj->ptmalfenster) gdk_cairo_set_source_pixbuf (cr, obj->pixbuf, 0, 0);
	cairo_paint (cr);
	return TRUE;
}

gboolean draw_window2 (GtkWidget *fenster, cairo_t *cr, CallbackObject *obj) {
//wird von gtk+ aufgerufen, maximiertes Matrixfenster
	gdk_cairo_set_source_pixbuf (cr, obj->pixbuf, 0, 0);
	cairo_paint (cr);
	return TRUE;
}

void printBestAgent(CallbackObject *ptobj) {
	ag *best=findBestAgent(ptobj);
	if (!best) {
		printf("kein best agent\n");
		return;
	}
	printf("Agent Nr %li\n", best->agnr);
	//Farbe ausgeben
	printf("color %i %i %i iyes %i %i %i ifyes %i %i %i  ifyes %i %i %i\n", best->bocolor.red, best->bocolor.green, best->bocolor.blue, \
		best->iycolor.red, best->iycolor.green, best->iycolor.blue, best->iyfcolor.red, best->iyfcolor.green, best->iyfcolor.blue, \
		best->bofcolor.red, best->bofcolor.green, best->bofcolor.blue);
	printf("Signatur= %s\n", best->Signatur);
	printf("mGeneration=%i\n", best->generationenMutZ);
	printf("Alter %li\n", ptobj->schritte-best->geburt);
	printProg(best->mem, best->memsize, ptobj);
	printf("Killpoints %i normal points %i\n", best->killpoints, best->peacefulpoints);
}

gboolean keypressed2(GtkWidget *w, GdkEvent *ev, CallbackObject *ptobj) {
//hier am besten alle Tastatureingaben für maximiertes Matrixfenster abfangen
	if (ev->key.keyval==GDK_KEY_Up) {
		ptobj->yorig-=100;
	}
	if (ev->key.keyval==GDK_KEY_Down) {
		ptobj->yorig+=100;
	}
	if (ev->key.keyval==GDK_KEY_Right) {
		ptobj->xorig+=100;
	}
	if (ev->key.keyval==GDK_KEY_Left) {
		ptobj->xorig-=100;
	}
	if (ev->key.keyval==GDK_KEY_G) {
		printBestAgent(ptobj);
	}
	if (ev->key.keyval==GDK_KEY_X) {
		char *ptstr=getStringOfSignatures();
		printf("%s", ptstr);
	}
	if (ev->key.keyval==GDK_KEY_Escape) {
		gtk_widget_hide(ptobj->ptmalfenster);
		ptobj->doPaintjob=false;
		gtk_toggle_button_set_active((GtkToggleButton *)gtk_builder_get_object(ptobj->builder, "arenazeigen"), false);
		gtk_toggle_button_set_active((GtkToggleButton *)gtk_builder_get_object(ptobj->builder, "max"), false);
		//printf("Escape\n");
	}
	if (ev->key.keyval==GDK_KEY_k) {
		long unsigned int allnut=0;
		for (int i=0; i<pow(glWorldsize,2); i++) {
			allnut+=(world+i)->Nutri;
		}
		printf("korrigiere Dichte. glMatrixpoints %i allnut=%li dif %li\n", glMatrixpoints, allnut, glMatrixpoints-allnut);
		glMatrixpoints=allnut;
	}
	ptobj->xorig=(ptobj->xorig+glWorldsize)%glWorldsize;
	ptobj->yorig=(ptobj->yorig+glWorldsize)%glWorldsize;
	glTriggerInitArena=1;
	//dichte korrigieren
	//initarena(ptobj);
	return TRUE;
}
gboolean keypressed(GtkWidget *w, GdkEvent *ev, CallbackObject *ptobj) {
//hier am besten alle Tastatureingaben für kleines Matrixfenster abfangen
	if (ev->key.keyval==GDK_KEY_Up) {
		ptobj->yorig-=100;
	}
	if (ev->key.keyval==GDK_KEY_Down) {
		ptobj->yorig+=100;
	}
	if (ev->key.keyval==GDK_KEY_Right) {
		ptobj->xorig+=100;
	}
	if (ev->key.keyval==GDK_KEY_Left) {
		ptobj->xorig-=100;
	}
	if (ev->key.keyval==GDK_KEY_G) {
		printBestAgent(ptobj);
	}
	if (ev->key.keyval==GDK_KEY_X) {
		char *ptstr=getStringOfSignatures();
		printf("%s", ptstr);
	}
	if (ev->key.keyval==GDK_KEY_Escape) {
		//printf("Escape\n");
	}
	if (ev->key.keyval==GDK_KEY_k) {
		long unsigned int allnut=0;
		for (int i=0; i<pow(glWorldsize,2); i++) {
			allnut+=(world+i)->Nutri;
		}
		printf("korrigiere Dichte. glMatrixpoints %i allnut=%li dif %li\n", glMatrixpoints, allnut, glMatrixpoints-allnut);
		glMatrixpoints=allnut;
	}
	ptobj->xorig=(ptobj->xorig+glWorldsize)%glWorldsize;
	ptobj->yorig=(ptobj->yorig+glWorldsize)%glWorldsize;
	glTriggerInitArena=1;
	//dichte korrigieren
	//initarena(ptobj);
	return TRUE;
}

void arenazeigen_toggled_cb (GtkToggleButton *ptbutton, CallbackObject *ptobj) {
//button "Arena zeigen" steuern. wird von gtk+ aufgerufen
	if (gtk_toggle_button_get_active(ptbutton)) {
		ptobj->doPaintjob=TRUE;
		//gtk_window_set_default_size((GtkWindow*)ptobj->ptmalfenster, 800, 600);
		gtk_widget_show_all(ptobj->ptmalfenster);
		initarena(ptobj);
	}
	else {
		//gtk_toggle_button_set_active(ptbutton,true);
		gtk_widget_hide(ptobj->ptmalfenster);
		ptobj->doPaintjob=FALSE;
	}
}

void max_toggled_cb(GtkToggleButton *ptbutton, CallbackObject *ptobj) {
	void resizeArena(int x, int y) {
		//gtk_toggle_button_set_active(ptbutton,false);
		g_object_unref(ptobj->pixbuf);
		free(ptobj->daten);
		ptobj->breite=x;
		ptobj->hoehe=y;
		ptobj->zeilenlaenge=3*ptobj->breite;
		ptobj->daten=g_new(guchar, ptobj->zeilenlaenge*ptobj->hoehe);
		ptobj->pixbuf=gdk_pixbuf_new_from_data \
			(ptobj->daten, GDK_COLORSPACE_RGB, FALSE, 8, ptobj->breite, ptobj->hoehe, ptobj->zeilenlaenge, NULL, NULL);
	}
	if (gtk_toggle_button_get_active(ptbutton)) {
		gtk_widget_hide(ptobj->ptmalfenster);
		ptobj->ptmalfenster=ptobj->ptmalfenster2;
		gtk_window_maximize((GtkWindow*)ptobj->ptmalfenster);
		resizeArena(ptobj->monitorgeox, ptobj->monitorgeoy);
		gtk_widget_show_all(ptobj->ptmalfenster);
		initarena(ptobj);
		ptobj->doPaintjob=true;
	}
	else {
		
		gtk_widget_hide(ptobj->ptmalfenster);
		ptobj->ptmalfenster=ptobj->ptmalfenster1;
		resizeArena(800,600);
		ptobj->doPaintjob=false;
	}
}

void byeclicked (GtkWidget *w, CallbackObject *ptobj) {
	gtk_main_quit();
}

bool threadsceduler (CallbackObject *ptobj);

void mainbuttons_cb (GtkToggleButton *ptbutton, CallbackObject *ptobj) { //wird von gtk aufgerufen, builder- Datei. *.glade
	if (gtk_toggle_button_get_active(ptbutton)) {
		g_source_remove(ptobj->tag);
		//"langsamer" button einstellen. lagwert= Pausen in millisekunden
		int lagwert=80;
		ptobj->tag=g_timeout_add(lagwert,(GSourceFunc)threadsceduler,ptobj);
	}
	else {
		g_source_remove(ptobj->tag);
		ptobj->tag=g_idle_add((GSourceFunc)threadsceduler,ptobj);
	}
}

void redistributeAgents(CallbackObject *ptobj) {
//verteile wenn nötig die Agenten neu auf die Threads. 
	//teste, ob Neuverteilung nötig ist
	bool operate=false; //wenn true, ausführen
	//-wenn neu Gestorbene beinahe größer ist als Anzahl Agenten pro thread
	static unsigned long olddead=0;
	static int numagsperth=0;
	if (numagsperth==0) {
		numagsperth=ptobj->anz/glNumThreads;
	}
	if (glZLfNr-olddead-ptobj->anz>numagsperth/2) { //wenn die Anzahl der in Zwischenzeit verstorbenen großer ist als numagsperth/4 geändert v62. bug?
		numagsperth=ptobj->anz/glNumThreads;
		operate=true;
		olddead=glZLfNr-ptobj->anz;
	}
	//jetzt noch ausführen
	if (operate) {
		distributeAgents(ptobj,0);
		numagsperth=0;
	}
}

void killAllAgs() {
//alle Agenten abschalten und recyclen
	for (int i=0; i<glNumThreads; i++) {
		tevthreads *ptmythread=glptevthreads[i];
		while (ptmythread->ptfirstag) ptmythread->ptfirstag=g_list_delete_link(ptmythread->ptfirstag, ptmythread->ptfirstag);
	}
}

void contitermrestart(CallbackObject *ptobj) {
//entscheide, ob Programm weiterlaufen soll, beendet oder neu gestartet
	void executexeptionbestags(CallbackObject *ptobj, char *nachricht) {
		if (glTestlaufBestAgs) {
			if (glBestAgs) {
				printf("Nachricht= %s\n", nachricht);
				printf("Restart mit nächstem Agenten für glTestlaufBestAgs\n");
				ptobj->schritte=0;
				initWorld();
				initFlood(ptobj);
				killAllAgs();
				ptobj->anz=0;
				initAgs(ptobj, 0);
				for (int i=0; i<glNumThreads; i++) glptevthreads[i]->schritt=0;
				//distributeAgents(ptobj,1);
			}
			else {
				printf("contitermrestart text= %s\n ", nachricht);
				printf(nachricht);
				exit (0);
			}
		}
		else {
			printf("%s\n", nachricht);
			exit(0);
		}
	}
//Anfang
	//-Abbruch, wenn keine Zellteilungen
	static long int lastlfnr=0;
	static int nochange=0;
	if (lastlfnr==glZLfNr) nochange++;
	else {
		lastlfnr=glZLfNr;
		nochange=0;
	}
	int glnochangelimit=20000;
	int glnochangelimitschrittmax=100000;
	if (ptobj->schritte<glnochangelimitschrittmax && nochange>glnochangelimit) {
		char nachricht[1000];
		sprintf(nachricht, "Abbruch, keine neuen Agenten seit %i Schritten. Schritte= %li\n", glnochangelimit, ptobj->schritte);
		executexeptionbestags(ptobj,nachricht);
	}
	//-Abbruch bei Erreichen von glMaxSchritte
	if (glMaxSchritte && (ptobj->schritte>glMaxSchritte)) {
		char nachricht[1000];
		sprintf(nachricht,"Maximale Anzahl Schritte erreicht (glMaxSchritte) ->schritte= %lu. Tschüss.\n", ptobj->schritte);
		executexeptionbestags(ptobj, nachricht);
	}
	//Abbruch, wenn Anzahl der Agenten==0 ist
	if (ptobj->anz==0) {
		executexeptionbestags(ptobj, "Ausgestorben\n");
	}
}

void testprobalines() {
//teste, wie viele zufällige Progzeilen ausgeführt werden können, dh als fehlerfrei erkannt werden
	ag *ptbest=findBestAgent();
	if (!ptbest) return;
	progline *ptprogline=malloc(sizeof(progline));
	int z=0;
	for (int l=0; l<10000; l++) {
		int line=g_rand_int_range(glptGrand, 0, ptbest->memsize/sizeof(progline));
		for (int i=0; i<sizeof(progline); i++) {
			*((char*) ptprogline+i)=g_rand_int_range(glptGrand, 0, UCHAR_MAX);
		}
		//bool lineisexecutable(progline *ptl, ag *ptag, int lnr) {
		if (lineisexecutable(ptprogline, ptbest, line)) z++;
	}
	printf("testprobalines %i\n", z);
}

bool threadsceduler (CallbackObject *ptobj) {
//setze threads in evschrittth in Gang, wenn die Bedingungen hierfür erfüllt sind
//muss true zurückliefern, wenn thread weiterlaufen soll (gtk+)
	//Abschlussarbeiten für einen Schritt
	g_mutex_lock(&glmsced);
	ptobj->schritte++;
	contitermrestart(ptobj); //Abbruchbedinigungen
	//-etc
	paintjob(ptobj);
	redistributeAgents(ptobj);
	//-save best agent
	static long int lastrunags=0;
	static bool firstrun=true;
	if (firstrun) {
		firstrun=false;
		lastrunags=glptevthreads[0]->ptobj->schritte;
	}
	//--beste Agenten speichern
	if (glptevthreads[0]->ptobj->schritte-lastrunags>glSafeBestAt) {
		if (saveBestAgent()) lastrunags=glptevthreads[0]->ptobj->schritte;
	}
	//und hier finally das Eigentliche: unlock alle
	for (int i=0; i<glNumThreads; i++) {
		g_mutex_unlock(&glptevthreads[i]->mutex);
	}
	regnen(ptobj); 
	return true;
}

char * makeSigPraefix() {
//einen Präfix für Signaturen kreieren
	char ckons[]="bcdfghjklmnpqrstvwxz";
	char cvok[]="aeiouy";
	int numletters=g_rand_int_range(glptGrand,4,7);
	char *ptsig=malloc(sizeof(char)*numletters+1);
	bool usekons=g_rand_int_range(glptGrand, 0, 2);
	for (int i=0; i<numletters; i++) {
		if (usekons){
			ptsig[i]=ckons[g_rand_int_range(glptGrand, 0, sizeof(ckons)-1)];
			usekons=false;
		}
		else {
			ptsig[i]=cvok[g_rand_int_range(glptGrand,0, sizeof(cvok)-1)];
			usekons=true;
		}
	}
	ptsig[numletters]=0;
	return ptsig;
}

int main (int argc, char **argv) {
	gtk_init(&argc, &argv);
	CallbackObject *ptobj=initCallbackObj();
	evinit(ptobj, argc, argv);//alle eigenen Inits
	printf("Starte Threadsceduler\n");
	ptobj->tag=g_idle_add((GSourceFunc)threadsceduler, ptobj); //große Schleife aufrufen
	g_timeout_add(1000, (GSourceFunc) stats, ptobj); // Statistikfenster schreiben
	printf("starte gtk_main\n");
	gtk_main();
	g_print("\n");
}

