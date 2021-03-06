#define F_CPU 16000000UL						
#include <avr/io.h>		
#include <stdio.h>
#include <avr/interrupt.h>					
#include <util/delay.h>
int16_t distance  = 0;  //variavel para registar a distancia do sonar antes de estar calibrada
uint16_t cont = 0 ;    // variavel para guardar o valor do TCNT/contador
int distance_final =0;// variavel ja com a distancia calibrada
 


typedef struct
{
	unsigned char usart:1; //flag para a comunicação usart com bluetooth
	unsigned char echo_pin1 :1; //flag que verifica se o echo1 esta ligado
	unsigned char echo_pin2 :1; //flag que verifica se o echo2 esta ligado
	unsigned char echo_pin3 :1;//flag que verifica se o echo3 esta ligado
	unsigned char trigger_pin1 :1;//flag que verifica se o trigger1 esta ligado
	unsigned char trigger_pin2 :1;//flag que verifica se o trigger2 esta ligado
	unsigned char trigger_pin3 :1;//flag que verifica se o trigger3 esta ligado
	unsigned char pisca1:1;//flag controlo do apito do buzzer conforme a distancia
	unsigned char pisca2:1;//flag controlo do apito do buzzer conforme a distancia
	unsigned char pisca3:1;//flag controlo do apito do buzzer conforme a distancia
	unsigned char sentido1:1;// troca o sentido de rotação M1
	unsigned char sentido2:1;// troca o sentido de rotação M2
	
}FLAGS_st;


char buffer[500];
volatile FLAGS_st flags = {0,0,0,0,0,0,0,0,0,0,1,0};
volatile unsigned char cnt_timer = 25,cont_timer1=12,cont_timer2=38,cont_timer3=60; 
//cnt_timer = 25, gera uma interrup de 500ms, para o led pisca pisca
//cont_timer1=12, gera uma interrupção de aproximadamente 250ms para o controlo do apito do buzzer
//cont_timer1=38, gera uma interrupção de aproximadamente 750ms para o controlo do apito do buzzer
//cont_timer1=60, gera uma interrupção de aproximadamente 1200ms para o controlo do apito do buzzer
volatile unsigned char pwm,pwm1;

unsigned char receive_buffer;
													
void inicio()			
{
	DDRD|=(1<<DDD7);       //trigger1 como saida
	DDRB|=(1<<DDB5);      //trigger2 como saida
	DDRB &=~(1<<DDB0);   //echo1 como entrada
	DDRB &=~(1<<DDB4);  //echo2 como entrada

 	PORTB |= (1 << PORTB0);	   //echo1 ligado
	PORTB |= (1 << PORTB4);	  //echo2 ligado
	PORTC |= (1<< PORTC0);	 //echo3 ligado
		
	TCCR1A |= 0b00000000;   //Normal mode (Counter), TOV1 Flag (Overflow) set on MAX
	TCCR1B |= 0b00000000;  //TIMER1 desligado, ira ser iniciado na interrupção externa
	
	TCNT1 = 0;            //Iniciar contador a 0;
	
	//---------------SONAR1-----------------------------------
	PCICR |= (1 << PCIE0);               // PIN change interrupt enable 0
	PCIFR |=(1<<PCIF0);
	PCMSK0 |= (1 << PCINT0)|(1<<PCINT4);//ATIVAR a interrupção no Pino do Echo1 e Echo2
	
	//----------------------Sonar3-----------------------------------
	DDRD|=(1<<DDD2);              //Trigger3_saída
	DDRC &=~(1<<DDC0);           //Echo3_entrada
	PCICR |= (1 << PCIE1);      // PIN change interrupt enable 1
	PCIFR |=(1<<PCIF1);
	PCMSK1 |= (1<<PCINT8);     // ATIVAR a interrupção no Pino do Echo3 PORTC0
	
	//------------------LED PISCA----------------------------------
	DDRD|= (1<<DDD5);                      //Define LED como saida, (piscar)
	
	//TIMER0
	TCCR0A |= (1<<WGM01);                 //modo CTC, interrupção de 500ms
	TIMSK0 |= (1<<OCIE0A)|(1<<OCIE0B);
	TCCR0B |= (1 << CS02)|(1 << CS00);   // PS = 1024
	OCR0A = 155;                        //Prescaler 1024; T=20ms
	OCR0B =155;
//-----------------BUZZER------------------------------------------
	DDRC|=(1<<DDC1);    //Buzzer como saída
//-------------------Motor1-------------------------------------------
	DDRB|=(1<<DDB3);                   //Controlo pwm
	DDRC|=(1<<DDC5);
	DDRC|=(1<<DDC4);                 //IN1->PC5,IN2->PC4, configurar como saída
	PORTC|=(0<<DDC4)|(1<<DDC5);	//INICIALMENTE RODA NESTE SENTIDO
	TCCR2A|=(1<<COM2A1)|(1<<COM2B1)|(1<<WGM21)|(1<<WGM20);//Fast PWM, Clear OC0A e OC0B compare match
	TCCR2B|=(1<<CS20);// no prescaling
	OCR2A=0;//Motor A desligado
	pwm=0;
//--------------------Motor2_controlo de direção-------------------------------------------	
	DDRC|=(1<<DDC3);                               
	DDRC|=(1<<DDC2);                              //IN3->PC3,IN4->PC2, configurar como saída
	DDRD|=(1<<DDD3);                             //definir como saída, PORTD3, controlo do pwm
	PORTC|=(0<<DDC3)|(1<<DDC2);	            //INICIALMENTE RODA NESTE SENTIDO
	OCR2B=0;                                   //Motor B desligado
	pwm1=0;                                   //Iniciado a 0,
//------------INICIALIZAÇÔES USART--------------------------	
	UCSR0A=0;
	UCSR0B |= (1 << RXCIE0) |(1 << RXEN0) | (1 << TXEN0);	//RX Complete Interrupt Enable;Receiver Enable;Transmitter Enable
	UCSR0C |= (1 << UCSZ01)| (1 << UCSZ00); 	       //8 bit data ; 1 stop bit 
	UBRR0L =103; 					
	UBRR0H = 0;		
	sei();                                               //iniciar as inicializações 
}

ISR(TIMER0_COMPA_vect) //Interrupção do TIMER0
{
	
	if (cnt_timer==0)              //verifica se o timer está a 0
	{
		PORTD ^= (1<<PORTD5); //Faz o toggle do Pino, para ligar ou desligar o led
		cnt_timer = 25;      //coloca o contador a 25 
	}
	else                        // se o contador for diferente de 0, decrementa-o
	cnt_timer--;
}

ISR(TIMER0_COMPB_vect)
{
	 if (flags.pisca1==1)                   // se a flag for a 1
	 {
		 if(cont_timer1==0)            //verifica se o timer esta a 0
		 {
			PORTC ^= (1<<PORTC1); //faz o toggle do PINO, para o buzzer
			cont_timer1 = 12;    //coloca o contador a 12 	
		 }
		else                        // se o contador for diferente de 0, decrementa-o
		{
			 cont_timer1--;
		}
	 }
	 if (flags.pisca2==1)                   // se a flag for a 1
	 {
		 if(cont_timer2==0)            //verifica se o timer esta a 0
		 {
			 PORTC ^= (1<<PORTC1);//faz o toggle do PINO, para o buzzer 
			 cont_timer2 = 38;   //coloca o contador a 38
		 }
		 else                       // se o contador for diferente de 0, decrementa-o
		 {
			 cont_timer2--;
		 }
	 }
	  if (flags.pisca3==1)                   // se a flag for a 1
	  {
		  if(cont_timer3==0)            //verifica se o timer esta a 0
		  {
			  PORTC ^= (1<<PORTC1);//faz o toggle do PINO, para o buzzer 
			  cont_timer3 = 60;   //coloca o contador a 60
		  }
		  else                       // se o contador for diferente de 0, decrementa-o
		  {
			  cont_timer3--;
		  }
	  }
}

ISR(USART_RX_vect)                         //interrupção pra a o USART
{
	flags.usart=1;                    //coloca a flag a 1
	receive_buffer = UDR0;           //a variavel receive_buffer fica com o valor do UDR0
	
}
void send_message(void)           // Envio de de dados atraves da USART
{
	unsigned char i=0;
	while(buffer[i]!='\0')
	{
		while((UCSR0A & 1<<UDRE0)==0);
		UDR0=buffer[i];
		i++;
	}
}
void processar_bluetooth(void)        // processar as funções
{
	switch(receive_buffer)
	{
		case 'l':

		flags.trigger_pin1=1;
		sprintf(buffer,"Distancia-Sensor_Left-Back = %d cm\n\n",distance_final);
		send_message();
		_delay_us(20);
		if(receive_buffer=='4'){
		sprintf(buffer,"STOP");
		send_message();
		flags.usart=0;
		
		}
		break;
		
		case 'r':
			
		flags.trigger_pin2 =1;
		sprintf(buffer,"Distancia-Sensor_Right-Back = %d cm \n\n",distance_final);
		send_message();
		_delay_us(20);
		if(receive_buffer=='4')
		{
			sprintf(buffer,"STOP");
			send_message();
			flags.usart=0;
		}
		break;
		
			
		case '1':
			
		flags.trigger_pin3=1;
		sprintf(buffer,"Distancia-Sensor_Front = %d cm\n\n",distance_final);
		send_message();
		_delay_us(20);
		if(receive_buffer=='4')
		{
			sprintf(buffer,"STOP");
			send_message();
			flags.usart=0;
		}
		break;
			
		case 'i':
				
		if (flags.sentido1==1)
		{
			PORTC|=(1<<DDC4)|(1<<DDC5);
			pwm=0;
			OCR2A=0;
			PORTC ^=(1<<DDC5);
			flags.sentido1=0;
			flags.usart=0;
		}
		else
		{
			PORTC|=(1<<DDC4)|(1<<DDC5);
			pwm=0;
			OCR2A=0;
			PORTC ^=(1<<DDC4);
			flags.sentido1=1;
			flags.usart=0;
		}
				
		break;
		
			case 't':
			case 'T':
			
			if (flags.sentido2==0)
			{
				PORTC|=(1<<DDC3)|(1<<DDC2);
				pwm1=0;
				OCR2B=0;
				PORTC ^=(1<<DDC2);
				flags.sentido2=1;
				flags.usart=0;
			}
			else
			{
				PORTC|=(1<<DDC3)|(1<<DDC2);
				pwm1=0;
				OCR2B=0;
				PORTC ^=(1<<DDC3);
				flags.sentido2=0;
				flags.usart=0;
			}
		break;
		
	}
}
	


void processa_echo1()
{
	if(PORTB0==0)
	{
		flags.echo_pin1=1;
	}
		flags.echo_pin1=0;

}
void processa_echo2()
{
	if(PORTB4==0)
	{
		flags.echo_pin2=1;
	}
		flags.echo_pin2=0;

}
void processa_echo3()
{
	if(PORTC0==0)
	{
		flags.echo_pin3=1;
	}
		flags.echo_pin3=0;
}

ISR(PCINT0_vect)
{	

	if(flags.echo_pin1==1 || flags.echo_pin2==1)
	{	
		
		TCCR1B=0;// Para o contador
		cont=TCNT1;//guarda o valor do contador
		distance=(cont*0.034)/2;//calculo da distancia
		distance_final=((distance-9.6)/15.824)+1; //distancia ja calibrada
		TCNT1=0;//coloca a 0
		flags.echo_pin1=0;
		flags.echo_pin2=0;
	}
	else
	{	
		TCNT1=0;
		flags.echo_pin1=1;
		flags.echo_pin2=1;
	}

}
ISR(PCINT1_vect)
{

	if(flags.echo_pin3==1 )
	{
		
		TCCR1B=0;// Para o contador
		cont=TCNT1;//guarda o valor do contador
		distance=(cont*0.034)/2;//calculo da distancia
		distance_final=((distance-9.6)/15.824)+1; //distancia ja calibrada
		TCNT1=0;//coloca a 0
		flags.echo_pin3=0;
	}
	else
	{
		TCNT1=0;
		flags.echo_pin3=1;
	}
}

void processar_trigger1()
{
	if (flags.trigger_pin1==1 )
	{
	PORTD|=(1<<PORTD7);//liga o trigger
	TCCR1B|=(1<<CS10);//inicia o contador
	_delay_us(10);// espera o 10us
	PORTD &=~(1<<PORTD7);//coloca trigger a 0
	flags.trigger_pin1=0;	
	processa_echo1();
	}

	
}
void processar_trigger2()
{
	if (flags.trigger_pin2==1 )
	{
	PORTB|=(1<<PORTB5);//liga o trigger
	TCCR1B|=(1<<CS10);//inicia o contador
	_delay_us(10);// espera os 10us
	PORTB &=~(1<<PORTB5);//coloca trigger a 0
	flags.trigger_pin2=0;
	processa_echo2();
	}
	
	
}
void processar_trigger3()
{
	if (flags.trigger_pin3==1 )
	{
		PORTD|=(1<<PORTD2);//liga o trigger
		TCCR1B|=(1<<CS10);//inicia o contador
		_delay_us(10);// espera os 10us
		PORTD &=~(1<<PORTD2);//coloca trigger a 0
		flags.trigger_pin3=0;
		processa_echo3();
	}
}
void buzzer()
{
	if (distance_final>=5 && distance_final<10)
	{
		flags.pisca1=0;
		flags.pisca2=0;
		flags.pisca3=0;	 	 
		PORTC|=(1<<PORTC1);//o buzzer liga continuamnete
	}
	if (distance_final>=10 && distance_final<15)
	{
		flags.pisca1=1;
		flags.pisca2=0;
		flags.pisca3=0;	 
	}
	if (distance_final>=15 && distance_final<20)
	{	 
		flags.pisca2=1;
		flags.pisca1=0;
		flags.pisca3=0;
	}
	if (distance_final>=20 && distance_final<30)
	{
		flags.pisca3=1;
		flags.pisca1=0;
		flags.pisca2=0;
	}
	if (distance_final>=30)
	{
		flags.pisca3=0;
		flags.pisca1=0;
		flags.pisca2=0;
		PORTC &=~ (1<<PORTC1);//Desliga Buzzer
	}  
}


	void motor_1()
	{
		if((flags.usart==1)&& (receive_buffer=='+'))
		{
			if (flags.sentido1==1)
			{
				if (pwm<110)
				{
					pwm=110;
				}
				else
				pwm=pwm+5;
			}
			else
			{
				if (pwm==0)
				{
					pwm=0;
				}
				else
				pwm=pwm-10;
			}
			OCR2A=pwm;
			flags.usart=0;
		}
		if((flags.usart==1)&& (receive_buffer=='-'))
		{
			if (flags.sentido1==0)
			{
				if (pwm<110)
				{
					pwm=110;
				}
				else
				pwm=pwm+5;
			}
			else
			{
				if (pwm==0)
				{
					pwm=0;
				}
				else
				pwm=pwm-10;
			}
			OCR2A=pwm;
			flags.usart=0;
		}
		if ((flags.usart==1)&& (receive_buffer=='0'))
			{
				pwm=0;
				OCR2A=0;
				flags.usart=0;
			}
}
void motor2()
{
	if((flags.usart==1)&& (receive_buffer=='d'))
	{
		if (flags.sentido2==1)
		{
			pwm1=pwm1+20;
		}
		else
		{
			if (pwm1==0)
			{
				pwm1=0;
			}
			else
			pwm1=pwm1-10;
		}
		OCR2B=pwm1;
		flags.usart=0;
	}
	if((flags.usart==1)&& (receive_buffer=='e'))
	{
		if (flags.sentido2==0)
		{
			pwm1=pwm1+20;
		}
		else
		{
			if (pwm1==0)
			{
				pwm1=0;
			}
			else
			pwm1=pwm1-10;
		}
		
		OCR2B=pwm1;
		flags.usart=0;
	}
}

int main(void)
{
	inicio();
	while(1)
	{		
			processar_trigger1();
			processar_trigger2();
			processar_trigger3();
			motor_1();
			motor2();
			buzzer();
			
		if (flags.usart==1)
		{
			processar_bluetooth();
		}
	}
	return 0;
}
