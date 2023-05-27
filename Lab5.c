#include<xc.h> //Libreria para PIC
#include<stdio.h> //Libreria adicional para manejo de decimales
#include "LibLCDXC8.h" //Libreria para uso de LCD

#pragma config FOSC=INTOSC_EC //Sentencia para usar oscilador externo
#pragma config WDT=OFF //Apagar el perro guardian
#pragma config PBADEN=OFF 
#pragma config LVP=OFF

#define _XTAL_FREQ 8000000 //Frecuencia de reloj
#define DATA_DIR TRISC2
#define DATA_IN RC2
#define DATA_OUT LATC2

unsigned char Temp, Hum;
unsigned char TempEEPROM;//Para el bonus
unsigned int A=0,B= 0;//valor de entrada para seleccionar unidades de temperatura



//Prototipos de las funciones
void LeerHT11(void);
unsigned char LeerByte(void);
unsigned char LeerBit(void);
void Transmitir(unsigned char);
unsigned char Recibir(void);
void TransmitirDatos(unsigned int Ent1, unsigned int Ent2);
void ColorRGB(unsigned int val);
unsigned int Conversion(unsigned char);
void GuardarDatoEnEEPROM(unsigned int dir, unsigned char Dato);
unsigned char leerDatoEnEEPROM(unsigned int dir);

//void LedPot(void);

void main(void) {

    //CONFIGURACION PARA EL RS232
    ConfiguraLCD(4);
    OSCCON = 0b01110110; //Establece el reloj interno en 8Mhz
    __delay_ms(1);
    DATA_OUT = 0;
    TXSTA = 0b00100000; //Configuraci?n del transmisor, habilitaci?n del transmisor y modo asincr?nico, bajas velocidades
    RCSTA = 0b10010000; //Configuraci?n del receptor, habilitaci?n del modulo EUSART, se habilita el receptor
    BAUDCON = 0b00000000; //Configuracion del modulo adc, no inversion logica, divisor de frecuencia 8bits, modo bajo consumo desactivado,
    //autodeteccion de velocidad off.
    TRISE=0;
    SPBRG = 12; //Valor para la vel de transmisi?n de datos, revisar formula -> SPBRG = 8M/(64*9600)-1
    //ConfigADC
    ADCON0 = 0b00000001;
    ADCON1 = 14;
    ADCON2 = 0b10001000; //Justificacion a derecha, adquisicion instantanea
    //Fin config ADC
    //CONFIGURACION DE PUERTOS I/O
    TRISB = 0b00001110; //Colocar puerto B como salida
    TRISD = 0; //Colocar puerto D como salida
    TRISA0 = 1; //Colocar pines A00 como entrada digital para ADC
    TRISC = 0b11010111; //Colocar Pines C0 y C1 como entrada (seleccion de temperatura) C2- SENSOR, C4-Selector cambio unidad envio datos RC6 como entrada TX, para lectura RC7 RX
    USBEN = 0;//habilita RC4 y RC5 desabilitando modulo USB
    UTRDIS = 1;
    RBPU = 0; //Activar resistencias pull up
    
    TempEEPROM = leerDatoEnEEPROM(0);
    
    InicializaLCD(); //Funcion para configuracion inicial del LCD
    //Timer0 interrupcion
    T0CON=0b00000011;//No habilita timer0, 16 bits de resolucion, reloj interno
    TMR0IF=0;// apaga bandera
    TMR0=3036; // valor pre carga
    TMR0IE=1; //Habilita la interrupcion 
    GIE=1; //habilita interrupciones globales
    TMR0ON=1;//Habilita la interrupcion Timer0, primer bit de T0CON
    //Fin de configuracion para Timer0
    BorraLCD(); //Limpiar el LCD

    if (TempEEPROM != 0xFF) {
        MensajeLCD_Var("Ultima temp:");
        DireccionaLCD(192);
        EscribeLCD_c(TempEEPROM / 10 + 48);
        EscribeLCD_c(TempEEPROM % 10 + 48);
        EscribeLCD_c(223);
        EscribeLCD_c('C');
        __delay_ms(2000);
        BorraLCD();
    }
    
    MensajeLCD_Var("Iniciando"); //Escribir mensaje de bienvenida
    __delay_ms(500); //Retraso para evitar errores
    EscribeLCD_c('.');
    __delay_ms(500);
    EscribeLCD_c('.');
    __delay_ms(500);
    EscribeLCD_c('.');
    __delay_ms(500);
    BorraLCD();
    __delay_ms(500);
    
    MensajeLCD_Var("Temp:");
    __delay_ms(500);
    DireccionaLCD(0xC0);
    MensajeLCD_Var("Hum:");
    while (1) {
        __delay_ms(500);
        //LATD0=1; LED?
        __delay_ms(500);
        //LATD0=0;
        LeerHT11();
        __delay_ms(100);
        //IniFlag = 1;
        GuardarDatoEnEEPROM(0, Temp);
        __delay_ms(100);
        ColorRGB(Temp); 
        __delay_ms(100);
        
        if(!RC4) TransmitirDatos(RB1, RB2);
        else TransmitirDatos(A, B);
        Conversion(0);
        RB0 = (ADRES <= 511) ? 0 : 1; //2.5*(2^10-1)/5 
    }
}

void LeerHT11(void) {
    //Por defecto el pin de comunicaci?n est? en alto, para iniciar la comunicaci?n se debe poner la l?nea de datos en bajo durante 18ms
    DATA_DIR = 0; //Configura el pin como salida, por defecto su valor de salida es 0
    __delay_ms(18); //Se esperan los 18ms
    DATA_DIR = 1; //Se reestablece el pin a entrada digital
    //Ahora se espera la respuesta del sensor
    while (DATA_IN == 1); //Tiempo en alto mientras el sensor responde
    __delay_us(120); //Pulso bajo, respuesta del sensor 80us, posteriormente pulso en alto de una duraci?n similar.
    while (DATA_IN == 1); //Tiempo en alto que dura hasta que el sensor toma control del canal de comunicaci?n
    //Recepci?n de datos
    Hum = LeerByte();
    LeerByte();
    Temp = LeerByte();
    LeerByte();
    LeerByte();
}

unsigned char LeerByte(void) {
    unsigned char res = 0, i;
    for (i = 8; i > 0; i--) {
        res = (res << 1) | LeerBit(); //Va moviendo los digitos del byte a la izquierta y a?adiendo los valores le?dos
    } //Comienza 00000000, lee 1, entonces 0000001, lee 0, entonces 00000010, lee 1, entonces 00000101, hasta llenar el byte
    return res;
}

unsigned char LeerBit(void) {
    unsigned char res = 0;
    while (DATA_IN == 0);
    __delay_us(13);
    if (DATA_IN == 1) res = 0; //Si el pulso dura menos de 30 us el bit es 0
    __delay_us(22);
    if (DATA_IN == 1) { // Sino, el bit es 1
        res = 1;
        while (DATA_IN == 1);
    }
    return res;
}

void Transmitir(unsigned char BufferT) {
    while (TRMT == 0);
    TXREG = BufferT;
}

unsigned char Recibir(void){
    while(RCIF==0);
    return RCREG;
}

void TransmitirDatos(unsigned int Ent1, unsigned int Ent2) {
    unsigned int n = Ent1 * 10 + Ent2, TempC = Temp, HumC = Hum;
    unsigned int Simb = 67;
    //BorraLCD();
    switch (n) {
        case 00://Celsius
            TempC = Temp;
            Simb = 67; //C
            break;
        case 01://Kelvin
            TempC = Temp + 273;
            Simb = 75; //K
            break;
        case 10://Rankine
            TempC = Temp * 9 / 5 + 492;
            Simb = 82; //R
            break;
        case 11://Fahrenheit
            TempC = Temp * 9 / 5 + 32;
            Simb = 70; //F
            break;
    }
    Transmitir('T');
    Transmitir('e');
    Transmitir('m');
    Transmitir('p');
    Transmitir(':');
    Transmitir(' ');

    DireccionaLCD(0x85);
    if (TempC / 100 > 0) {
        Transmitir(TempC / 100 + 48);
        EscribeLCD_c(TempC / 100 + 48);
        TempC = TempC % 100;
    }
    Transmitir(TempC / 10 + 48);
    Transmitir(TempC % 10 + 48);
    if (Simb==70 | Simb == 67) Transmitir(223);
    Transmitir(Simb);
    Transmitir(' ');
    Transmitir('\n');
    Transmitir('H');
    Transmitir('u');
    Transmitir('m');
    Transmitir(':');
    Transmitir(' ');
    Transmitir(Hum / 10 + 48);
    Transmitir(Hum % 10 + 48);
    Transmitir(' ');
    Transmitir('%');
    Transmitir('\n');
    Transmitir(' ');
    //Transmision LCD
    EscribeLCD_c(TempC / 10 + 48);
    EscribeLCD_c(TempC % 10 + 48);
    if (Simb==70 | Simb == 67) EscribeLCD_c(223);
    EscribeLCD_c(Simb);
    EscribeLCD_c(' ');
    DireccionaLCD(0xC4);
    EscribeLCD_c(Hum / 10 + 48);
    EscribeLCD_c(Hum % 10 + 48);
    EscribeLCD_c('%');

}

void ColorRGB(unsigned int val) {
    if (val < 22) {
        RB5 = 0;
        RB6 = 0;
        RB7 = 0;
    } else if (val >= 22 && val < 25) {
        RB5 = 1;
        RB6 = 0;
        RB7 = 1;
    } else if (val >= 25 && val < 28) {
        RB5 = 1;
        RB6 = 0;
        RB7 = 0;
    } else if (val >= 28 && val < 31) {
        RB5 = 1;
        RB6 = 1;
        RB7 = 0;
    } else if (val >= 31 && val < 34) {
        RB5 = 0;
        RB6 = 1;
        RB7 = 0;
    } else if (val >= 34 && val < 37) {
        RB5 = 0;
        RB6 = 1;
        RB7 = 1;
    } else if (val >= 37 && val < 42) {
        RB5 = 0;
        RB6 = 0;
        RB7 = 1;
    } else if (val >= 42) {
        RB5 = 1;
        RB6 = 1;
        RB7 = 1;
    }
}

unsigned int Conversion(unsigned char canal) {
    ADCON0 = 0b00000001 | (canal << 2);
    GO = 1; //bsf ADCON0,1
    while (GO == 1);
    return ADRES;
}

void GuardarDatoEnEEPROM(unsigned int dir, unsigned char dato) {
    EEADR = dir;
    EEDATA = dato;
    //Configuracion EECON1
    EEPGD = 0; //Selecciona la memoria EPROM
    CFGS = 0; //Accede a la configuracion de EEPROM
    WREN = 1; //Habilita la escritura

    GIE = 0; //Desabilita interrupciones, recomendacion de la datasheet (INTCON)
    //Secuencia obligatoria del puerto EECON2
    EECON2 = 0x55;
    EECON2 = 0xAA;
    WR = 1; //Permite la escritura
    GIE = 1; //Rehabilita interrupciones,
    while (!EEIF);
    EEIF = 0; //Apaga la bandera
    WREN = 0; //Desabilita la escritura

}

unsigned char leerDatoEnEEPROM(unsigned int dir) {
    EEADR = dir; //Establece la direccion a leer
    EEPGD = 0; //Selecciona la memoria EPROM
    CFGS = 0; //Accede a la configuracion de EEPROM
    RD = 1; //Activa lectura

    return EEDATA;
}

void __interrupt() ISR(void){
    if(TMR0IF){
        TMR0IF=0;
        RE2 = !RE2;
        TMR0 = 3036;//Precarga 2^n - Tsobreflujo*Fbus_Timer0/PreScaler
        //Tuvo que usarse una resolucion de 16 bits y un PS de  para lograr el valor deseado
    }
    if(RCIF){
        switch (Recibir()){
                case 'C':
                    A=0;
                    B=0;
                    break;
                case 'K':
                    A=0;
                    B=1;
                    break;
                case 'R':
                    A=1;
                    B=0;
                    break;
                case 'F':
                    A=1;
                    B=1;
                    break;
                default:
                    break;
        }
        __delay_ms(100); 
    }            
}
 