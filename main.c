/* 
 * File:   main.c
 * Author: maiso
 *
 * Created on 18 mai 2020, 16:38
 */

#include <stdio.h>
#include <stdlib.h>
#include "init.h"
#include "MI2C.h"
//#include "interruption.h"
#include "fonction.h"
#include <p18f2520.h>
#pragma config OSC = INTIO67
#pragma config PBADEN = OFF, WDT = OFF, LVP = OFF, DEBUG = ON

struct Statut Etat;
void HighISR(void);

#pragma code HighVector=0x08
void IntHighVector(void)
{
    _asm goto HighISR _endasm
}
#pragma code

#pragma interrupt HighISR 
void HighISR(void)
{
    //Etat->MDG = 0;
    if(PIR1bits.TMR2IF)     //PWM Timer2
    {
        PIR1bits.TMR2IF=0;
    }
    if(INTCONbits.INT0IF)   //T�l�commande
    {
        INTCONbits.INT0IF = 0 ;
        Ecrire_i2c_Telecom(0xA2,0x31);//demande � la t�l�commande
        while(Detecte_i2c(0xA2));
        Lire_i2c_Telecom(0xA2,Etat.touche);// on vient recuperer la touche qui a �t� appuy�e
        /*on teste quelle touche a �t� appuy�e*/
        if (Etat.touche[1] == 0x33)
        {
            if(Etat.START == 1)
               Etat.START = 0;
            else
               Etat.START = 1;
        }
        if(Etat.START)
            printf("D�marrage du syst�me\r\n");
        else
            printf("Arr�t du syst�me\r\n");
    }
    if(PIR1bits.ADIF==1)    //Batterie
    {
        PIR1bits.ADIF=0;
        if(ADCON0bits.CHS == 2) //On v�rifie que le channel est sur Vbat pour �viter de mesurer des valeurs de IRD/G
        {
            ADCON0bits.GO=0;
            Etat.SommeMesures += ADRESH*256+ADRESL&0x0000FFFF; //&0x0000FFFF
            //printf("SommeMesures : %ld\r\n",Etat.SommeMesures);
            Etat.nbMesure++;
            affichageLED(&Etat);
            if(Etat.nbMesure == 4)
            {
                Etat.Vbat = Etat.SommeMesures/4;
                printf("Vbat : %ldV\r\n", Etat.Vbat*12/65472); //
                if(Etat.Vbat < 54464)    //43 200 = 10V
                {  
                    Etat.START = 0;
                    printf("Batterie faible\r\n",Etat.Vbat);
                }
                Etat.SommeMesures = 0;
                Etat.nbMesure = 0;
                affichageLED(&Etat);
            }
        }
    }
    if(INTCONbits.TMR0IF)       //Timer0 qui contr�le la fr�quence des mesures batterie
    {
        INTCONbits.TMR0IF=0; 
        TMR0H=0x0B;             //Rechargement
        TMR0L=0xDC;            
        ADCON0bits.GO=1;        //On autorise une mesure
        Etat.Timer0 = ~Etat.Timer0;
        affichageLED(&Etat);
    }
}


void main(void) 
{
    initPIC();
    initINT();
    initStatut(&Etat);
    while(1)
    {
        while(Etat.START == 0)
        {
            INTCONbits.TMR0IE=0;    //disable it
            PIE1bits.TMR2IE=0;
            PIE1bits.ADIE=0;
        }
        INTCONbits.TMR0IE=1;        //enable it 
        PIE1bits.TMR2IE=1;
        PIE1bits.ADIE=1;
        while(INTCONbits.TMR0IF == 0);  //On fait une d�tection toute les quart de seconde pour �conomiser la batterie
        if(detectionObjet())
        {
            printf("Objet d�tect�\r\n");
            Etat.Objet = 1;
            printf("D�marrage moteur\r\n");
            PWM(20);
            Etat.Moteurs = 1;
        }
        else
        {
            Etat.Objet = 0;
            PWM(0);
            Etat.Moteurs = 0;
        }
        /*S�quence d'arr�t*/
        if(Etat.START == 0)
        {
            PWM(0);
            initStatut(&Etat);
        }
        affichageLED(&Etat);
    }
}
