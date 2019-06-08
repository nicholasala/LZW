//Sviluppata da Agnola Tommaso
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <locale.h>
#include <time.h>
#include <math.h>

///////////////////////////////// VARIABILI GLOBALI ////////////////////////////////////////////////////

int MAX = 1048576; 					//porzione di file presa ad ogni passaggio, espressa in numero di byte
unsigned int maxD = 0; 				//prossimo indice del dizionario, aggiornato ad ogni caricamento di un elemento
int iP = 0; 						//cursore porzione in decompressione
int iP2 = 0; 						//cursore porzione in decompressione
unsigned short scaricaI[1048576]; 	//vettore di presalvataggio
unsigned char scaricaO[1048576]; 	//vettore di presalvataggio
short valido = 1; 					//controllo validita' indice in arrivo in decompressione
int limite;
int LIMITE=65535;					//valore massimo che pu' assumere un indice. In seguito viene cancellato il dizionario. Equivale a 2^16 perchè scriviamo sempre 16 bit

///////////////////////////////// FUNZIONI COMPRESSIONE ////////////////////////////////////////////////////

void scaricaFinale(FILE *fptr)						//scarico elementi residui alla fine dell'algoritmo
{
    fwrite(scaricaI, 2, iP, fptr);
}

void aggiungiASalva(unsigned short x, FILE *fptr)	//scrittura bufferizzata per velocizzare le prestazioni. Mano a mano si riempie un buffer da MAX e quando è pieno lo si scrive
{
    if(iP==MAX)
    {

        fwrite(scaricaI, 2, MAX, fptr);
        iP=0;
    }

    scaricaI[iP]=x;
    iP++;
}

void bufferizza(unsigned char x, FILE *fptr)		//scrittura bufferizzata utilizzata in decompressione; stessa logica di quella in compressione
{
    if(iP2==MAX)
    {
        fwrite(scaricaO, 1, MAX, fptr);
        iP2=0;
        
    }

    scaricaO[iP2]=x;
    iP2++;
}
void scaricaFinale2(FILE *fptr)							//scarico elementi residui alla fine dell'algoritmo. Tale funzione è necessaria dato che i byte residui presenti nel buffer non multipli della dimensione massima del buffer devono essere scritti
{
    fwrite(scaricaO, 1, iP2, fptr);
}

///////////////////////////////// DIZIONARIO COMPRESSIONE ////////////////////////////////////////////////////

typedef struct nodo{		//struttura che rappresenta un nodo
    unsigned int id;		//indice univoco che rappresenta il nodo e di fatto anche la parola che ha come ultimo carattere tale nodo
    struct nodo **figli;	//puntatore a un array di nodi (i figli!)
}Nodo;

void inizializzaDizionarioC(Nodo **radici){						//funzione che inizializza il dizionario con i 256 caratteri ASCII
    for(int i=0; i<256; i++)
    {
        radici[i] = (Nodo *)malloc(sizeof(Nodo));
        radici[i]->id = i;
        radici[i]->figli=(Nodo **)calloc(256,sizeof(Nodo *));
    }
    maxD = 256;													//il prossimo nodo avrà indice 256
}

void svuotaDiz(Nodo *n){										//funzione richiamata una volta raggiunto il massimo numero di indici rappresentabile
    if(n!=NULL){												//tale funzione svuota l'albero con una cancellazione pseudo post-order dei nodi. Scorre l'albero verso il basso fino all'ultimo figlio istanziato e, risalendo, lo libera 
        for(int i=0;i<256;i++){
            if(n->figli[i]!=NULL){
                svuotaDiz(n->figli[i]);							//richiamo la funzione sui figli istanziati
            }
        }
        free(n->figli);											//libero la memoria accupata dall'array di figli
        free(n);												//libero la memoria occupata dal nodo
    }
}

void svuotaDizionario1(Nodo **radici)							//funzione per cancellare l'albero dalle radici
{
    for(int i=0;i<256;i++){
        svuotaDiz(radici[i]);
    }
}

//////////////////////////////////FUNZIONI PAROLA///////////////////////////////////////////////////

typedef struct carattere{										//struttura utilizzata solo in decompressione, rappresenta la parola				
    unsigned char carattere;									//carattere della parola
    struct carattere *punC;										//puntatore al prossimo carattere della parola
}carattereD;

carattereD *creaParola(unsigned char c)							//funzione che crea la parola partendo dal primo carattere. Con la funzione aggiungiCarattereAParola si aggiungeranno i successivi
{
    carattereD *p;
    p=(carattereD *)malloc(sizeof(carattereD));
    p->carattere=c;
    p->punC=NULL;
    return p;
}
carattereD *cercaParola(carattereD *dizionario[], unsigned int i)	//cerca la parola all'interno del dizionario dato l'indice. Ritorna la parola se presente, NULL se non presente
{
    if(i<=limite)
        return dizionario[i];
    return NULL;
}
void inizializzaDizionarioD(carattereD *dizionario[])				//inzializza il dizionario con i 256 caratteri ASCII
{
    limite=255;
    unsigned int i;
    for(i=0;i<=limite;i++)
    {
        dizionario[i]=creaParola(i);
    }

}
void eliminaParola(carattereD *p)									//eliminazione della parola liberando carattere per carattere
{																	
    if(p!=NULL)
    {
        if(p->punC != NULL){										//richiama la funzione finche viene raggiunto l'ultimo carattere della parola e vengono liberati ricorsivamente
            eliminaParola(p->punC);
        }
        free(p);
    }
}
void svuotaDizionario(carattereD *dizionario[])						//funzione che svuota tutte le parole presenti nel dizionario...
{
    for(int i=0;i<limite;i++){
        eliminaParola(dizionario[i]);								//...richiamando la funzione precedentemente esposta per ogni parola dell'array dizionario
    }
}
void aggiungiCarattereAParola(carattereD *p, unsigned char c)		//aggiunge un carattere a una parola già esistente
{
    carattereD *prec=p;
    while(p != NULL)												//scorre la parola fino a raggiungere l'ultimo carattere e viene quindi aggiunto il carattere passato alla funzione, aggiornando i puntatori
    {
        prec=p;
        p = p->punC;
    }
    p=(carattereD *)malloc(sizeof(carattereD));
    prec->punC=p;
    p->carattere=c;
    p->punC=NULL;
}
carattereD *creaParolaCopia(carattereD *parola)						//ritorna una parola copia rispetto a quella passata come argomento. La utilizziamo per non modificare in seguito quella originale
{
    carattereD *finale = creaParola(parola->carattere);
    parola = parola->punC;

    while(parola != NULL){
        aggiungiCarattereAParola(finale, parola->carattere);
        parola = parola->punC;
    }
    return finale;
}
void aggiungiParolaADizionario(carattereD *dizionario[], carattereD *parola)	//aggiunge la parola all'aaray dizionario
{
    limite++;
    dizionario[limite]=parola;

}

///////////////////////////////// COMPRESSIONE ////////////////////////////////////////////////////

void comprimi(char *nomeInput,char *nomeCompresso){
    Nodo **radici=(Nodo **)malloc(256*sizeof(Nodo *));						//alloco la memoria per l'array di nodi radici
    inizializzaDizionarioC(radici);											//inizializzo il dizionario

    Nodo **n = radici; 														//nodo di navigazione albero

    FILE *file;
    file = fopen(nomeInput, "rb");											//apro il file da leggere in modalità lettura binaria
    FILE *compresso;
    compresso = fopen(nomeCompresso, "wb");									//apro il file su che diverrà il file compresso in modalità scrittura binaria				
    if(file){
        unsigned int i=0; 													//numero byte presi da fread, numero caratteri corrispondenza trovata, cursore creazione finestra, limite finestra
        unsigned int x = 0; 												//prossimo indice da salvare
        unsigned char porzione[MAX];										//buffer di lettura
        do{
            i=fread(porzione, sizeof(unsigned char), MAX, file);			//viene letta dal file originale una porzione di grandezza MAX e viene caricata nel buffer di lettura
            for(int k=0; k<i; k++){											//scorre carattere per carattere i caratteri letti
                unsigned char e = porzione[k]; 								//carattere elaborato

                if(n[e] == NULL){											//se non presente...
                  Nodo* aggiunto = (Nodo *)malloc(sizeof(Nodo));			//creo nuovo nodo
                  aggiunto->figli = (Nodo **)calloc(256,sizeof(Nodo *));
                  aggiunto->id = maxD;										//vi assegno l'indice univoco che rappresenta il nodo e la parola che ha come ultimo carattere tale nodo

                  n[e] = aggiunto;											//aggiungo il nuovo figlio al dizionario e prelevo l'indice da salvare nel file compresso
                  aggiungiASalva(x, compresso);

                  n = radici;												//tramite il nodo di navigazione torno alla radice dell'albero
                  maxD++;
                  k--;														//torno indietro di un carattere
                }else{
                  x = n[e]->id;												//in x salvo l'indice dell'ultimo nodo trovato
                  n = n[e]->figli;											//con la modifica di n scorro il dizionario di un livello sotto
                }

                if(maxD==(LIMITE)){											//se l'indice ha raggiunto il massimo rappresentabile con i bit utilizzati viene cancellato il dizionario e successivamente reinizializzato
                    svuotaDizionario1(radici);
                    inizializzaDizionarioC(radici);
                }
            }
        }while(i==MAX);
        if(ftell(file)!=0)
        	aggiungiASalva(x, compresso); 										//salvo l'ultimo indice elaborato
        scaricaFinale(compresso);											//scrivo sul file i bit residui (dato che il file non è per forza un multiplo della grandezza del buffer)
    }else{    printf("Errore");        }
    fclose(file);															//chiusura dei file
    fclose(compresso);
}

///////////////////////////////////////DECOMPRESSIONE////////////////////////////////////////////////

void deComprimi(char *nomeCompresso,char *nomeDecompresso)
{					
    unsigned int dim=LIMITE;
    carattereD *dizionario[dim];															//creo il dizionario che può essere lungo al massimo LIMITE dato che quando si raggiunge tale valore si andrà a svuotare
    inizializzaDizionarioD(dizionario);														// precarica il dizionario delle radici con i 256 possibili byte //visualizzaDizionario(dizionario);
    FILE *file;
    file = fopen(nomeCompresso, "rb");														//apro il file da decomprimere
    FILE *deCompresso;
    deCompresso = fopen(nomeDecompresso, "wb"); 											//apro il file su cui verrà scritto il file decompresso
    if(file)																				//eseguo la lettura solo se il file e' esistente
    {
    	for(int i=0;i<MAX;i++)																//inizializzo il buffer di scrittura a 0											
    		scaricaO[i]=0;
        int f = 0;																			//variabile flag che mi indica se è la prima volta che eseguo le operazioni. Questo perche, non avendo una parola precedente dalla quale partire si deve comporatre in modo diverso
        unsigned int i=0;																	//variabile che indica il numero di blocchi da 16 bit letti
        unsigned int cond=0;
        unsigned char c;
        unsigned short porzione[MAX];														//buffer di lettura. Al posto di leggere un blocco per volta si riempie uno o più buffer dai quali preleveremo i blucchi volta per volta
        carattereD *precedente = (carattereD *)malloc(sizeof(carattereD));					//parola del passaggio precedente
        carattereD *corrente = (carattereD *)malloc(sizeof(carattereD));					//parola del passaggio attuale
        do{
            i=fread(porzione, 2, MAX, file); 												//i corrispondera' al limite di byte esistenti in porzione
            for(int k=0; k<i; k++)															//ciclo che passa porzione ottenuta
            {
                corrente = cercaParola(dizionario, porzione[k]);							//ricerca della parola dato l'indice. L'indice è rappresentato da porzione[k]
                if(corrente==NULL)															//se la parola non esiste (caso specifico di LZW dovuto al fatto che il decompressore è un passo indietro rispetto al compressore)
                {																			//La nuova parola è la vecchia parola seguita dal primo carattere della parola stessa
                    corrente=creaParolaCopia(precedente);									//creo una nuova parola per non modificare quella originale
                    aggiungiCarattereAParola(corrente, corrente->carattere);				//aggiungo, come esposto prima, il primo carattere della parola alla parola stessa
                }
                if(f)																		//se è la prima volta in cui si legge un blocco aggiungo alla precedente il carattere letto e viene inserita nel dizionario la parola precedente
                {
                    aggiungiCarattereAParola(precedente,corrente->carattere);
                    aggiungiParolaADizionario(dizionario, precedente);
                }
                precedente = creaParolaCopia(corrente);										//precedente diventa corrente dato che le operazioni si ripeteranno
                while(corrente != NULL)														//viene inserita la parola corrente, carattere per carattere, nel buffer di scrittura sul file finale
                {
                    c=corrente->carattere;
                    bufferizza(c,deCompresso);
                    corrente = corrente->punC;
                }
                if((limite+1)==(LIMITE))													//limite tiene traccia delle parole aggiunte al dizionario. Se sono presenti LIMITE-1 parole esso viene svuotato e successivamente reinizializzato
                {
                    svuotaDizionario(dizionario);
                    inizializzaDizionarioD(dizionario);
                }
                f=1;																		//flag posto a 1--> non è più la prima volta che si legge un blocco
            }
        }while(i==MAX);
        scaricaFinale2(deCompresso);														//vengono scritti i byte residui, non multipli di della grandezza del buffer
    }else{  printf("Errore");       }
    fclose(file);																			//chiusura dei file
    fclose(deCompresso);
}

int main(int argc, char *argv[])
{
    clock_t tic,toc;
    char stringa[30];
    if(argc>1)
    {
    	if((strcmp(argv[1],"-c")==0)&&argc==4)
	    {
	        tic=clock();																				//funzione per misurare le tempistiche
	        comprimi(argv[2],argv[3]);
	        toc=clock();
	        printf("\n\ntempo compressione in secondi: %f\n",((double)toc-tic)/CLOCKS_PER_SEC);
	    }
	    else if((strcmp(argv[1],"-d")==0)&&argc==4)
	    {
	        printf("\nDecompressione!\n");
	        tic=clock();
	        deComprimi(argv[2],argv[3]);
	        toc=clock();
	        printf("\n\ntempo decompressione in secondi: %f\n",((double)toc-tic)/CLOCKS_PER_SEC);
	    }
	    else
	    {
	        printf("\nErrore nella scelta!\n");
	    }
    } 
}
