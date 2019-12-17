/*
 * Weather station.c
 *
 * Created: 01.12.2019 12:04:50
 * Author : Group 10
 */ 

#define F_CPU 16000000UL
#define LCD_MAX_COLS	16
#define LCD_MAX_ROWS	2
#define ADC_PIN0 0 // for rain sensor
#define ADC_PIN1 1 // for hum sensor
#define ADC_PIN2 2 // for air p sensor
#define ADC_PIN3 3 // for LDR

//include standard libraries
#include <avr/io.h>
#include <stdio.h>
#include <util/delay.h>
#include <string.h> 

//include user defined libraries
#include "lcd.h"
#include "i2cmaster.h"
#include "ds1621.h"

//structures
typedef	struct
{
	double min;
	double max;
	double current;	
	char name[16];
	char unit[16];
}values_t;

typedef struct  
{
	char option0[16];
	char option1[16];
	char condition;
}text_t;

//function prototypes

void LCD_yes_no_backlight(); 
void button_buffer(char button_pressed);
void selected_screen();
void general_screen();
void change_min_max();
void update_values();
void alarm_check();
unsigned int adc_read(unsigned char adc_channe);
void presettings_for_values();
//global variables

//for buttons:
char up = 0x0C;
char down = 0x0A;
char enter = 0x06;

//for sensor values:
values_t values[3];
/*
values[0]	values for
values[1]	values for humidity
values[2]	values for air_pressure
*/
text_t day_night = {"     Night     ","      Day      ",1};
text_t rain = {"    No Rain     ","    Raining    ",1};
	
//for timer and lcd back light control
unsigned int timecounter = 0;
unsigned int timecounter_max = 2000; // changing this value will increase/decrease the time it takes for the lcd light turns on/ff
unsigned int period_counter = 0; 
//for screen controlling
char current_screen = 0;
char number_of_screens = 3;
char level_of_screen = 0;
unsigned char counter = 10; // counting up to 10 and then reads the value of air p/hum
/*
screen numbers and their meanings

for number_of_screen
0 general screen
1 temperature screen
2 air pressure screen
3 humidity screen

for level_of_screen

0 same screen as given for current_screen
1 the selected measurement of current_screen and option to change the min value for the respective alarm
2 the selected measurement of current_screen and option to change the max value for the respective alarm
*/

// start main 
int main()
{
	DDRC = 0xF0; // PC0…3 as inputs, for sensors
	DDRB = 0xF1; // PB1-3 as inputs for buttons
	DDRD = 0xFF; // PD2 as output for buzzer
	PORTC = 0x3F; 
	PORTB = 0x0E; 
	PORTD = 0x00; //buzzer off at start

	ADMUX = (1<<REFS0);
	//set prescaler to 128 and turn on the ADC module
	ADCSRA = (1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0)|(1<<ADEN);
	
	i2c_init();//communication tool (needs to be first bc lcd and temp. sensor rely on this)
	LCD_init();//lcd	
	ds1621_init();//temperature sensor
	presettings_for_values();
	update_values();
	while(1)
	{
		selected_screen();
		timecounter++;
		LCD_set_cursor(0,0);
		if (timecounter>timecounter_max) LCD_yes_no_backlight(); //turning back light on if any button is pressed without changing any value/screen
		if (PINB == up)
		{	
			LCD_yes_no_backlight();
			current_screen++;
			if (current_screen > number_of_screens) current_screen = 0;
			selected_screen();
		}//if (PINB == up)
		if (PINB == down)
		{
			LCD_yes_no_backlight();
			if (current_screen == 0) current_screen = number_of_screens;
			else current_screen--;
			selected_screen();
		}//if (PINB == down)
		if (PINB == enter && current_screen!=0)//going on another lvl of current screen to change min value (lvl 1) or max value (lvl 2) or going back to normal value screen (lvl 0)
		{
			LCD_yes_no_backlight();
			level_of_screen++;
			change_min_max();
		}//if (PINB == enter && current_screen!=0)
		LCD_yes_no_backlight();
		update_values();
		alarm_check();
		button_buffer(up);
		button_buffer(down);	
	}//while(1)
}//end main




//functions

//setting the lcd background on/off
void LCD_yes_no_backlight()
{
	if(timecounter>timecounter_max) LCD_no_backlight();
	else LCD_backlight();
}

//preventing sending more than 1 button value over period of time (basically a one time edge trigger)
void button_buffer(char button_pressed)
{
	while (1)
	{
		if (PINB == button_pressed) timecounter = 0;
		else break;
	}//while(1)
}

//changing between screens
void selected_screen()
{
	unsigned char cs = current_screen - 1;
	if (current_screen == 0) general_screen();
	else
	{
		LCD_set_cursor(0,0);
		printf("%s",values[cs].name);
		LCD_set_cursor(0,1);
		printf("    %.1lf%s        ",values[cs].current,values[cs].unit);
	}//else of (current_screen == 0) general_screen();
}

//different screens
void general_screen()
{
	LCD_set_cursor(0,0);
	if (day_night.condition == 0) printf("%s",day_night.option0);
	else printf("%s",day_night.option1);
	LCD_set_cursor(0,1);
	if (rain.condition == 0) printf("%s",rain.option0);
	else printf("%s",rain.option1);
}

//changing min and max alarm values
void change_min_max()
{
	unsigned char cs = current_screen - 1;  //cs is used since current_screen has the following values: 0 1 2 3, but values_t values[3] only has [0] [1] [2]
	button_buffer(enter);
	
	if (level_of_screen == 1)//lvl 1 for min values
	{
		while (1)
		{
			timecounter++;
			LCD_set_cursor(0,0);
			printf("min. value:     %d",cs);
			LCD_set_cursor(0,1);
			printf("   %.1lf %s    ",values[cs].min,values[cs].unit);
			if (PINB == up)
			{
				values[cs].min+=0.1;
				timecounter = 0;
			}//if (PINB == up)
			if (PINB == down)
			{
				values[cs].min-=0.1;
				timecounter = 0;
			}//if (PINB == down)
			if (values[cs].min > values[cs].current)
			{
				LCD_set_cursor(0,0);
				printf ("min value can't");
				LCD_set_cursor(0,1);
				printf("exceed current");
				while (PINB != down) values[cs].min = values[cs].current - 0.1;
				LCD_clear();
			}//if (values[cs].min > values[cs].max)
			if (PINB == enter)
			{
				level_of_screen++;
				button_buffer(enter);
				break;
			}
			LCD_yes_no_backlight();
			update_values();
			alarm_check();
			_delay_ms(30);
		}//while(1),  for min value
	}//if (level_of_screen = 1)
	if (level_of_screen == 2)//lvl 2 for max values
	{
		while (1)
		{
			timecounter++;
			level_of_screen = 0;
			LCD_set_cursor(0,0);
			printf("max. value:    ");
 			LCD_set_cursor(0,1);
			printf("   %.1lf %s   ",values[cs].max,values[cs].unit);
			if (PINB == up)
			{
				values[cs].max+=0.1;
				timecounter = 0;
			}//if (PINB == up)
			if (PINB == down)
			{
				values[cs].max-=0.1;
				timecounter = 0;
			}//if (PINB == down)
			if (values[cs].max < values[cs].current)
			{
				LCD_set_cursor(0,0);
				printf ("max value can't");
				LCD_set_cursor(0,1);
				printf("undercut current");
				while (PINB != up) values[cs].max = values[cs].current + 0.1;
				LCD_clear();
				timecounter++;
			}//if (values[cs].max < values[cs].min)
			if (PINB == enter)
			{
				button_buffer(enter);
				selected_screen();
				break;
			}//if (PINB == enter)
			LCD_yes_no_backlight();
			update_values();
			alarm_check();
			_delay_ms(30);
		}//while(1) for max value
	}//if (level_of_screen == 2)
	else//in case of something happening which wasn't meant to be happening
	{
		LCD_clear();
		LCD_set_cursor(0,0);
		printf("Error");
		LCD_set_cursor(0,1);
		printf("change m&m: %c",level_of_screen);
	}//else, for errors within change_min_max and level_of_screen
	LCD_yes_no_backlight();
	update_values();
	alarm_check();
	button_buffer(enter);
}

//function to update all sensor values

void update_values()
{
	double a = 0.00876;
	double b = -0.48571;
	double c = 1023;
	counter++;
	/*
	ADC_PIN0 0  for rain sensor
	ADC_PIN1 1  for hum sensor
	ADC_PIN2 2  for airp sensor
	ADC_PIN3 3  for LDR
	*/
	//value[i] for 0 = temp; 1 = airp;  2 = hum;
	values[0].current = get_temperature();
	if (counter > 10)
	{
		counter = 0;
		values[1].current = (((adc_read(ADC_PIN2))/c) -b)/a;
		values[2].current = (5/(c) * adc_read(ADC_PIN1) - 0.826)/0.0315;	
	}//if (coutner > 5)
	
	//setting day_night condition  1 = day  0 = night
	if (adc_read(ADC_PIN3)>300) day_night.condition = 1;
	else day_night.condition = 0; 
	//setting rain condition 1 = rain 0 = no rain
	if (adc_read(ADC_PIN0)>500) rain.condition = 1;
	else rain.condition = 0;

}

//testing if current value of any sensor is exceeding its pre signed min/max value

void alarm_check()
{
	
	unsigned char alarm = 0;
	for (unsigned char i = 0;i <= 2;i++)
	{
		if (values[i].min > values[i].current)
		{
			LCD_set_cursor(0,0);
			printf("       LOW      ");
			LCD_set_cursor(0,1);
			printf("%s value",values[i].name);
			alarm = i + 1;
			level_of_screen = 1;
			break;
		}//if (values[i].min > values[i].current)
		if (values[i].max < values[i].current)
		{
			LCD_set_cursor(0,0);
			printf("      HIGH      ");
			LCD_set_cursor(0,1);
			printf("%s value",values[i].name);
			alarm = i + 1;
			level_of_screen = 2;
			break;
		}//if (values[i].min > values[i].current)
	}//for (char i = 0;i<=2;i++)
	while (alarm > 0)
	{
		_delay_ms(1); 
		period_counter++;
		LCD_backlight();
		timecounter = 0;
		if (period_counter == 250)
		{
			PORTD = ~PORTD; //turns on/off buzzer
			period_counter = 0;
		}
		if(PINB == up || PINB == down || PINB == enter)
		{
			if (level_of_screen == 1) values[current_screen - 1].min-= 3;
			else values[current_screen - 1].max+= 3;

			PORTD = 0x00; //turns off buzzer
			LCD_yes_no_backlight();
			current_screen = alarm;
			alarm = 0;
			change_min_max();
			break;
		}//if(PINB == up || PINB == down || PINB == enter)
	}//if (alarm > 0)
}

//reading the analog values from sensors

unsigned int adc_read(unsigned char adc_channe)
{
	ADMUX &= 0xf0; // clear any previously used channel, but keep internal reference
	ADMUX |= adc_channe; // set the desired channel
	//start a conversion
	ADCSRA |= (1<<ADSC);
	// now wait for the conversion to complete
	while ( (ADCSRA & (1<<ADSC)) );
	// now we have the result, so we return it to the calling function as a 16 bit unsigned int
	return ADC;
}

//starting conditions when microcontroller is turned on (good for testing)

void presettings_for_values()
{
	strcpy(values[0].name,"Temperature:    ");
	strcpy(values[0].unit,"\337C");
	strcpy(values[1].name,"Air pressure:  ");
	strcpy(values[1].unit,"kPa");
	strcpy(values[2].name,"Humidity:       ");
	strcpy(values[2].unit,"%");
	values[0].max = 30;
	values[0].current = 15;
	values[0].min = 10;
	values[1].max = 110;
	values[1].min = 90;
	values[2].max = 60;
	values[2].min = 20;
}