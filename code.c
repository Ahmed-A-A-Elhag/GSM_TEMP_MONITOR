#define F_CPU 8000000UL

#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



#define key_prt PORTB
#define key_ddr DDRB
#define key_pin PINB

unsigned char keypad[4][4]= {{'7','8','9','#'},{'4','5','6','#'},{'1','2','3','#'},{'*','0','#','#' }};
int setpoint_H;
int setpoint_L;

int Temperature1;
int Temperature2;
int rate;
int rate_threshold;
unsigned int count=0;

int num1=0;
int num2=0;
int num3=0;

void ADC_init(){

	ADCSRA = 0x87;
	ADMUX = 0xe0;
	DDRA = 0x00;

	return;
}

void usart_init(){
	UCSRC = (1<<URSEL) | (1<<UCSZ1) | (1<<UCSZ0);
	UCSRB = (1<<TXEN)| (1<<RXEN);
	UBRRL = 0x33;
}

unsigned char pressedKey(void)
{
	unsigned char colloc,rowloc;
	DDRB=0xff;
	key_ddr=0xf0;
	key_prt=0xff;
	while(1){
		do
		{
			key_prt &=0x0f;
			colloc=(key_pin & 0x0f);
			
		} while(colloc!=0x0f);
		
		do{
			
			do
			{
				_delay_ms(20);
				colloc=(key_pin& 0x0f);
				
			} while (colloc==0x0f);
			_delay_ms(20);
			colloc=(key_pin& 0x0f);
			
		}while (colloc==0x0f);
		
		while(1){
			
			key_prt =0xef;
			colloc=(key_pin & 0x0f);
			if(colloc != 0x0f){
				rowloc=0;
				break;
			}
			key_prt=0xdf;
			colloc=(key_pin & 0x0f);
			
			if(colloc != 0x0f){
				rowloc=1;
				break;
			}
			key_prt=0xbf;
			colloc=(key_pin & 0x0f);
			
			if(colloc != 0x0f){
				rowloc=2;
				break;
			}
			key_prt=0x7f;
			colloc=(key_pin & 0x0f);
			rowloc=3;
			break;
		}
		
		if(colloc == 0x0e)
		return (keypad[rowloc][0]);
		else if(colloc == 0x0d)
		return (keypad[rowloc][1]);
		else if(colloc == 0x0b)
		return(keypad[rowloc][2]);
		else
		return (keypad[rowloc][3]);
	}
}


void LCD_command(unsigned char temp){

	PORTC = temp; //screen lines connected to port c
	PORTD = 0b00010000; //E=1,RS=0,WR=0
	_delay_us(2);  //High to low pulse to latch command
	PORTD = 0b00000000;
	_delay_us(100);

	return;
}

void LCD_data(unsigned char temp){

	PORTC = temp;
	PORTD = 0b00010100;//RS=1
	_delay_us(2);
	PORTD = 0b00000100;//H TO L PULSE
	_delay_us(100);

	return;
}


void LCD_init(){

	DDRC = 0xff;
	DDRD = 0xff;
	PORTD = 0x00;//OUTPUT 00X00
	_delay_ms(2);
	LCD_command(0x38);//8 BIT ,5*7 MATRIX
	_delay_ms(2);
	LCD_command(0x0f);//display on cursor blinking
	_delay_ms(2);
	LCD_command(0x01);// clear lcd screen
	_delay_ms(2);
	LCD_command(0x06);// INCREMENT CURSOR TO RIGHT

	return;
}

void clearLCD(){

	LCD_command(0x01);// clear lcd screen
	_delay_ms(2);
	LCD_command(0x02);//return home 0,0
	_delay_ms(2);
	LCD_command(0x0f);//display on cursor blinking
	_delay_ms(2);
	LCD_command(0x06);//INCREMENT CURSOR TO RIGHT AFTER ENTERING A CHAR
	_delay_ms(2);

	return;
}

void gotoxy(unsigned char x, unsigned char y){   //X Column,starts from 1 ,Y row starts from 1

	unsigned char firstCharAdr[] = {0x80, 0xC0};    //80,C0 beginning of 1st and 2nd rows respectively
	LCD_command(firstCharAdr[y-1] + x - 1); //0,0
	_delay_ms(2);
	LCD_command(0x0f);//blinking cursor
	_delay_ms(2);
	return;
}

void LCD_string(char* str){

	int len = strlen(str), i;
	for(i = 0 ; i < len ; i++){
		LCD_data(str[i]);//display string on lcd
		_delay_ms(2);
	}
	return;
}
void usart_transmit(char data){
	while (UCSRA & (1<<UDRE)==0);//ucsra empty
	UDR = data;
	_delay_us(500);
}

void sendData(char* str){
	int len = strlen(str);
	for (int i = 0; i < len; i++ ){
		usart_transmit(str[i]);//send string to usart
		_delay_ms(20);
	}
}

void check_gsm(){
	sendData("AT\r");
	_delay_ms(50);
	
}


void text_mode(){
	sendData("AT+CMGF=1\r");//sms text mode
	_delay_us(500);
}


void message(char* sms,int Temperature){
	
	sprintf(sms, "current Temperature is  %dC ", Temperature);     // I added a space after %d
	sendData(sms);
	LCD_string(sms);      //print in LCD
	usart_transmit(13);  //enter
	_delay_ms(50);
}

void rate_message(char* msg, int rate){
	sprintf(msg, "Temperature rise rate is %dC/min ",rate);    // I added a space after %d
	sendData(msg);
	LCD_string(msg);
	usart_transmit(13);
	_delay_ms(50);
}


void send_cmgs(){     //message for threshold
	sendData("AT+CMGS=\"+249128884414\"\r");
	_delay_us(500);
}


void send_SMS(){
	char sms[50];
	text_mode();
	send_cmgs();
	_delay_us(1000);
	message(sms,Temperature1);
	usart_transmit(26); // ctrl_Z
	_delay_us(1000);
}

void rate_SMS()
{
	char msg [50]; //rate message holder
	text_mode();
	send_cmgs();
	rate_message(msg, rate);
	
	usart_transmit(26);  //ctrl z
	_delay_us(1000);
}


int main(){
	
	LCD_init();
	_delay_ms(100);

	char* setpoint_prompt_H = "Enter high setpoint: ";
	char* setpoint_prompt_L = "Enter Low setpoint: ";
	char* rate_threshold_prompt = "Enter rate: ";
	
	char tmp1 ;
	char tmp2;
	
	int i = 0;
	int j = 0;
    int k=0;
	
	char str_setpoint_H[4];        //store the value of setpoint from keypad
	char str_setpoint_L[4];   
	char str_rate_threshold[4];          //rate message
	
	
	//====================================
	
	LCD_string(setpoint_prompt_H);
	gotoxy(1,2);
	LCD_command(0x0f);
	_delay_ms(500);
	
	while((tmp1 = pressedKey()) != '#'){
		
		if(tmp1 != '*'){
			str_setpoint_H[i] = tmp1;
			i++;// no of setpoin entries
			LCD_data(tmp1);
		}
		else if(tmp1 == '*' && i > 0){
			i--;
			LCD_command(0x10);//cursor  to left
			_delay_ms(20);
			LCD_data(0);
			_delay_ms(20);
			LCD_command(0x10);
			_delay_ms(20);
			LCD_command(0x0f);
			_delay_ms(20);
		}
	}
	
	
	clearLCD();
	LCD_string(setpoint_prompt_L);
	gotoxy(1,2);
	LCD_command(0x0f);
	_delay_ms(500);
	
	while((tmp1 = pressedKey()) != '#'){
		
		if(tmp1 != '*'){
			str_setpoint_L[j] = tmp1;
			j++;// no of setpoin entries
			LCD_data(tmp1);
		}
		else if(tmp1 == '*' && i > 0){
			j--;
			LCD_command(0x10);//cursor  to left
			_delay_ms(20);
			LCD_data(0);
			_delay_ms(20);
			LCD_command(0x10);
			_delay_ms(20);
			LCD_command(0x0f);
			_delay_ms(20);
		}
	}
	
	
	
	_delay_ms(15);
	clearLCD();
	_delay_ms(500);
	LCD_string(rate_threshold_prompt);
	gotoxy(1,2);
	LCD_command(0x0f);

	while((tmp2 = pressedKey()) != '#'){
		
		if(tmp2 != '*'){
			str_rate_threshold[k] = tmp2;
			k++;
			LCD_data(tmp2);
		}
		else if(tmp2 == '*' && j > 0){
			k--;
			LCD_command(0x10);
			_delay_ms(20);
			LCD_data(0);
			_delay_ms(20);
			LCD_command(0x10);
			_delay_ms(20);
			LCD_command(0x0f);
			_delay_ms(20);
		}
	}
	setpoint_H = atoi(str_setpoint_H);    //convert to int
	setpoint_L = atoi(str_setpoint_L);    //convert to int
	rate_threshold = atoi(str_rate_threshold);
	_delay_ms(20);
	
	//====================================	
	LCD_init();
	_delay_ms(20);
	usart_init();
	_delay_ms(20);
	ADC_init();
	
	
	while(1){
		
		
		ADCSRA |= (1<<ADSC);//start conversion
		while ((ADCSRA & (1<<ADIF))==0);
		Temperature1 = ADCH;
      
		
		
		
		if(count==0)
		    Temperature2=Temperature1;
	    count++;	
		
		if (Temperature1 >= setpoint_H +2)
		{
			if (num1==0 )
			{
				clearLCD();
				check_gsm();
				send_SMS();
				num1=1;
			}
			
			
			
		}
	
		 
		if (Temperature1 <= setpoint_L -2)
		{
			if (num2==0 )
			{
				clearLCD();
				check_gsm();
				send_SMS();
				num2=1;
				
			}
			
		
		}
		
		//=============== Temperature rate
		if(count==60){
			rate =abs( Temperature1 - Temperature2);
			if(rate >= rate_threshold +5)
			{
				if (num3==0)
				{
					clearLCD();
					check_gsm();
					rate_SMS();
					num3=1;
				}
				
				
			}
			count=0;
		}
		_delay_ms(1000);
		ADCSRA |= (1<<ADIF);
		if (num1==1 && num2==1 &&  num3==1)
		{
			break;
		}
	}
	
	return 0;
}
