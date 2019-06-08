#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <locale.h>
#include <time.h>
#include <math.h>

///////////////////////////////// VARIABILI GLOBALI ////////////////////////////////////////////////////

int MAX = 1048576; //porzione di file presa ad ogni passaggio, espressa in numero di byte
unsigned long maxD = 0; //prossimo indice del dizionario, aggiornato ad ogni caricamento di un elemento
int nBit = 9;	//numero di bit necessari per l'indice attuale, aggiornato ad ogni caricamento di un elemento
int iS = 0; //cursore buffer savataggio
int iP = 0; //cursore porzione in decompressione
int iSCD = 0; //cursore vettori presalvataggio
short salva[32] = {0}; //vettore salvataggio indici a n bit variabile
unsigned int scaricaI[1048576]; //vettore di presalvataggio indici
unsigned char scaricaC[1048576]; //vettore di presalvataggio caratteri
short residui = 0; //controllo validita' indice in arrivo in decompressione
unsigned long LIMITE = 4294967295; //limite alla grandezza del dizionario, massimo numero rappresentabile da un unsigned long (2^32 - 1)

///////////////////////////////// FUNZIONI COMPRESSIONE ////////////////////////////////////////////////////

void aggiornaNBit(int fase){//funzione che aggiorna il numero di bit necessari al salvataggio dell'indice corrente
		if(fase){
			nBit = (int)(log2(maxD+1)+1);
		}else{
			nBit = (int)(log2(maxD)+1);
		}
}

unsigned int finestra(int min, int max)//ottiene un valore contenuto in salva
{
		int r = 0;
		for(int i=min;i<max;i++)
		{
			r += salva[i];
			if(i!=(max-1))
				r = r<<1;
		}
		return r;
}

void scarica(FILE *fptr) //ottiene un valore contenuto in salva, lo aggiungo al buffer di presalvataggio e se pieno scarica il buffer su file
{
	iS = 0;
	unsigned int x = finestra(0,32);

	if(iSCD==MAX)
	{
		fwrite(scaricaI, 4, MAX, fptr);
		iSCD=0;
	}

	scaricaI[iSCD]=x;
	iSCD++;
}

void scaricaFinale(FILE *fptr, unsigned int contaDizionari, char *nomeCompresso)//scarico elementi residui alla fine dell'algoritmo
{
	fwrite(scaricaI, 4, iSCD, fptr);
	unsigned int x = finestra(0,iS);
	fwrite(&x, (iS/8)+1, 1, fptr);

	//utilizzo varint per indicare il massimo indice del dizionario in decompressione
	rewind(fptr);
	FILE *compresso;
	compresso = fopen(nomeCompresso, "wb"); //creo file compresso
	int n = 0;
	if(nBit%7==0){
		n = nBit/7;
	}else{
		n = (nBit/7) + 1;
	}
	//aggiungo all'inizio del nuovo i byte per il varint che indichera' al decompressore il massimo indice del dizionario raggiunto
	unsigned char indicatore = 0;
	for(int y=n-1; y>=0; y--){
		if(y!=0){
			indicatore = 1<<7;
		}
		indicatore += ((maxD & (127 << y*7))>>y*7);
		fwrite(&indicatore, sizeof(unsigned char), 1, compresso);
		indicatore = 0;
	}

	//scrivo su file anche il numero di volte in cui e' stato svuotato il dizionario in fase di compressione
	fwrite(&contaDizionari, sizeof(unsigned char), 1, compresso);

	fclose(fptr);
	fptr = fopen("ftlzw.lzw", "rb");
	int i = 0;
	//copio tutto il file temporaneo nel file finale, utilizziamo questo metodo perche' tramite fwrite e' impossibile scrivere all'inizio del file senza modificare i byte iniziali
	do{
					i=fread(scaricaC, sizeof(unsigned char), MAX, fptr);
					fwrite(scaricaC, sizeof(unsigned char), i, compresso);
		}while(i==MAX);

	fclose(compresso);
	fclose(fptr);
	remove("ftlzw.lzw"); //rimuovo il file temporaneo
}

void aggiungiASalva(unsigned int x, FILE *fptr) //aggiungiamo all'array salva (array di short che corrispondono a cifre binarie) i 4 byte letti da file
{
	for(int i=1;i<=nBit;i++)
	{
		if(iS<32)//il buffer di salvataggio non e' pieno
		{
			salva[iS] = (x >> (nBit - i)) & 1;
			iS++;
		}
		else {	i--; scarica(fptr);	}
	}
}

///////////////////////////////// DIZIONARIO COMPRESSIONE ////////////////////////////////////////////////////

typedef struct nodo{ //struttura principale del dizionario
    unsigned int id;
    struct nodo **figli;
}Nodo;

void inizializzaDizionarioC(Nodo **radici){ //inizializziamo il dizionario ai 256 byte base
    for(int i=0; i<256; i++)
    {
        radici[i] = (Nodo *)malloc(sizeof(Nodo));
        radici[i]->id = i;
        radici[i]->figli=(Nodo **)calloc(256,sizeof(Nodo *));
    }
    maxD = 256;
		aggiornaNBit(0);
}

void svuotaDiz(Nodo *n){ //svuotiamo il dizionario
    if(n!=NULL){
        for(int i=0;i<256;i++){
            if(n->figli[i]!=NULL){
                svuotaDiz(n->figli[i]);
            }
        }
        free(n->figli);
        free(n);
    }
}

void svuotaDizionario1(Nodo **radici)
{
    for(int i=0;i<256;i++){
        svuotaDiz(radici[i]);
    }
}

///////////////////////////////// COMPRESSIONE ////////////////////////////////////////////////////

void comprimi(char *nomeFile, char *nomeCompresso){ //funzione principale di compressione
    Nodo **radici=(Nodo **)malloc(256*sizeof(Nodo *));
    inizializzaDizionarioC(radici);

    Nodo **n = radici; //nodo di navigazione albero

    FILE *file;
    file = fopen(nomeFile, "rb");
    FILE *compresso;
    compresso = fopen("ftlzw.lzw", "wb");

    if(file){
        unsigned int i=0; //numero byte presi da fread, numero caratteri corrispondenza trovata, cursore creazione finestra, limite finestra
        unsigned int x=0; //prossimo indice da salvare
				unsigned int contaDizionari=0; //numero di dizionari utilizzati
        unsigned char porzione[MAX];
        do{
            i=fread(porzione, sizeof(unsigned char), MAX, file);
            for(int k=0; k<i; k++){
                unsigned char e = porzione[k]; //byte elaborato

                if(n[e] == NULL){
                  //creo nuovo nodo
                  Nodo* aggiunto = (Nodo *)malloc(sizeof(Nodo));
                  aggiunto->figli = (Nodo **)calloc(256,sizeof(Nodo *));
                  aggiunto->id = maxD;

                  //aggiungo il nuovo figlio al dizionario e prelevo l'indice da salvare nel file compresso
                  n[e] = aggiunto;
                  aggiungiASalva(x, compresso);

                  //tramite il nodo di navigazione torno alla radice dell'albero
                  n = radici;
                  maxD++;
									aggiornaNBit(0);
                  k--;
                }else{
                  x = n[e]->id; //salvo l'indice di dov'e' il nodo navigatore adesso
                  n = n[e]->figli; //n passa ai figli successivi
                }

                if(maxD==(LIMITE)){ //se maxD raggiunge il limite svuotiamo il dizionario
                    svuotaDizionario1(radici);
                    inizializzaDizionarioC(radici);
										contaDizionari++;
                }
            }
        }while(i==MAX);
				if(ftell(file) != 0){
					aggiungiASalva(x, compresso); //salvo l'ultimo indice
				}
        scaricaFinale(compresso, contaDizionari, nomeCompresso);
    }else{    printf("Errore");        }
    fclose(file);
    fclose(compresso);
}

//////////////////////////////// DIZIONARIO DECOMPRESSIONE ////////////////////////////////////////////////////
typedef struct carattere{
    unsigned char carattere;
    struct carattere *punC;
}carattereD;
carattereD *prec;

void inizializzaDizionarioD(carattereD *dizionario[]){ //inizializziamo il dizionario
		unsigned int i;
		for(i=0;i<256;i++){
				dizionario[i]=(carattereD *)malloc(sizeof(carattereD));
				dizionario[i]->carattere = i;
		}
		maxD = 256;
		aggiornaNBit(1);
}

void aggiungiCarattereAParola(carattereD *p, unsigned char c) //data una parola aggiungiamo ad essa un carattere
{
	   carattereD *prec=p;

		 while(p != NULL)
	   {
	   	  prec=p;
	      p = p->punC;
	   }
	   p=(carattereD *)malloc(sizeof(carattereD));
	   prec->punC=p;
	   p->carattere=c;
}

void uguagliaParoleEAggiungi(carattereD *a, carattereD *b, unsigned char h) //ugualiamo due parole e aggiungiamo alla creata un carattere
{
	//porto in avanti la parola precedente, dato che in b abbiamo gia' il primo carattere di prec
	a = a->punC;
	while(a != NULL){
		aggiungiCarattereAParola(b, a->carattere);
		a = a->punC;
	}
	aggiungiCarattereAParola(b, h);
}

void aggiungiADizionario(carattereD *dizionario[], unsigned char c){ //aggiungiamo al dizionario la parola precedente piu' il carattere passato
	dizionario[maxD] = (carattereD *)malloc(sizeof(carattereD));
	dizionario[maxD]->carattere = prec->carattere;
	uguagliaParoleEAggiungi(prec,dizionario[maxD], c);
}

void identificaEAggiungiD(carattereD *dizionario[], unsigned int indice){
		//aggiungo nuova corrispondenza al dizionario e aggiorno prec con la corrispondenza attuale
		if(prec == NULL){
			prec = (carattereD *)malloc(sizeof(carattereD));
			prec = dizionario[indice];
		}else{
			if(indice == maxD){ //siamo nel caso parola creata al momento
					aggiungiADizionario(dizionario, prec->carattere);
			}else{
				  aggiungiADizionario(dizionario, dizionario[indice]->carattere);
			}

			maxD++;
			aggiornaNBit(1);
			prec = dizionario[indice];
		}
}

void eliminaParola(carattereD *p)
{
    if(p!=NULL)
    {
        if(p->punC != NULL){
            eliminaParola(p->punC);
        }
        free(p);
    }
}
void svuotaDizionario(carattereD *dizionario[])
{
    for(int i=0;i<LIMITE;i++){
        eliminaParola(dizionario[i]);
    }
}

///////////////////////////////// FUNZIONI DECOMPRESSIONE ////////////////////////////////////////////////////

unsigned int prelevaERitorna(unsigned char b, short *p){ //funzione utilizzata per il varint
	*p = b>>7;
	return (b & 127);
}

void aggiungiCompletoASalva(unsigned int x) //aggiungiamo i 4 byte divisi in bit all'array salva
{
	for(int i=0;i<32;i++)
	{
			salva[i] = (x >> 31-i) & 1;
	}
}

void carica(unsigned int porzione[], int *l)
{
	iP += 1;
	iS = 0;
	aggiungiCompletoASalva(porzione[iP]);
}

unsigned int prelevaIndice(unsigned int porzione[], int *len)//tramite questa funzione preleviamo un indice lungo nBit dall'array salva
{
	int l = *len;
	short c = 0;
	unsigned int r = 0;
	int limite = iS + nBit;

		for(int i=iS;i<limite;i++)
		{
			if(iS<32 && iP<l)
			{
				r = r<<1;
				r += salva[iS];
				iS++;
				c++;
			}
			else if(iP<l) //abbiamo letto tutto l'array salva, dobbiamo ricaricare salva per generare l'indice ma solo se non siamo alla fine della porzione caricata
			{
				carica(porzione, len);
				limite = (nBit - c) + 1;
				i = 0;
			}else
			{
				residui = c;//blocco la lettura, e mantengo i dati finora letti in r, cosi' che in caso di byte residui sara' possibile andare avanti con la decompressione
				break;
			}
		}

	return r;
}

void scaricaD(carattereD *s, FILE *fptr){ //scarichiamo nel buffer di presalvataggio o nel file se questo e' pieno
	while(s!= NULL){
		if(iSCD==MAX){
			fwrite(scaricaC, 1, MAX, fptr);
			iSCD=0;
		}

		scaricaC[iSCD]=s->carattere;
		iSCD++;

		s = s->punC;
	}
}

void scaricaFinaleD(FILE *fptr){
	fwrite(scaricaC, 1, iSCD, fptr);
}

///////////////////////////////// DECOMPRESSIONE ////////////////////////////////////////////////////

void deComprimi(char *nomeCompresso, char *nomeFile)
{
		FILE *compresso; //quale dimensione diamo al nostro dizionario ? il varint ci ritorna l'ultimo maxD
		compresso = fopen(nomeCompresso, "rb");

		//utilizzo di varint per prelevare dal file il massimo indice del dizionario
		short primob = -1;
	  unsigned int dim = 0, contaDizionari=0;
		unsigned char indicatore;

		while(primob == 1 || primob == -1){
			if(primob != -1)
				dim = dim << 7;

			fread(&indicatore, sizeof(unsigned char), 1, compresso);
			dim += prelevaERitorna(indicatore, &primob);
		}

		fread(&contaDizionari, sizeof(unsigned char), 1, compresso);

		if(contaDizionari!=0){ //cio' significa che in compressione il dizionario e' stato svuotato almeno una volta, assumiamo quindi la massima dimensione del dizionario
			dim = LIMITE;
		}

		carattereD *dizionario[dim];
		inizializzaDizionarioD(dizionario);

		FILE *deCompresso;
		deCompresso = fopen(nomeFile, "wb");

		int i=0;
		unsigned int x=0;
		unsigned int porzione[MAX];
		do{
				iP = 0;
				i=fread(porzione, 4, MAX, compresso);
				aggiungiCompletoASalva(porzione[iP]);//inizializzo salva
				do{
						x = prelevaIndice(porzione, &i); //prelevo l'indice
						identificaEAggiungiD(dizionario,x);

						if(residui == 0){
							scaricaD(prec, deCompresso);
						}else{
							//fprintf(stderr, "residui e' %d",residui);
						}

						if(maxD==(LIMITE))
						{
								svuotaDizionario(dizionario);
								inizializzaDizionarioD(dizionario);
						}
				}while(iP < i);
		}while(i==MAX);
		scaricaFinaleD(deCompresso);
		fclose(compresso);
		fclose(deCompresso);

		//MANCA DECOMPRESSIONE ULTIMI BYTE RESIDUI*/
}


int main(int argc, char *argv[]){
	clock_t tic,toc;
	if(strcmp(argv[1],"-c")==0)
	{
		printf("\nCompressione!\n");
		tic=clock();
		comprimi(argv[2], argv[3]);
		toc=clock();
		printf("\n\ntempo compressione in secondi: %f\n",((double)toc-tic)/CLOCKS_PER_SEC);
	}
	else if(strcmp(argv[1],"-d")==0)
	{
		printf("\nDecompressione!\n");
		tic=clock();
		deComprimi(argv[2], argv[3]);
		toc=clock();
		printf("\n\ntempo Decompressione in secondi: %f\n",((double)toc-tic)/CLOCKS_PER_SEC);
	}
	else
	{
		printf("\nErrore nella scelta!\n");
	}
	return 0;
}
