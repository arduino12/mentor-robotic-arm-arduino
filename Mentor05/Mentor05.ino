/*
	Mentor V5.0 Software!
	==========================
	Copyright (C) 2014 Arad Eizen (A.E Tec) LTD INC.
	This Program Controls The Mentor Robotic Arm Via Its Original Controller!
	Maybe Via UART (USB To Serial) from EXE Software Running On A PC .
	Main Board: Arduino Mega 2560.
	Language: cxx.
*/

#include <Wire.h>					// For LCD Display
#include <LiquidCrystal_I2C.h>		// For LCD Display
#include <EEPROM.h>					// For Saving Settings

const uint8_t
	MIN_AXES[]		= {0x06, 0x20, 0x00, 0x4E, 0x4E, 0x10},
	MAX_AXES[]		= {0xF4, 0xED, 0xC0, 0xCB, 0xCB, 0x70},
	HOME_POINT[]	= {0x87, 0x97, 0x20, 0x5A, 0x5A, 0x10},
	TEST_POINT[]	= {0x87, 0x90, 0x35, 0x99, 0x99, 0x40},
	
	AXIS_COUNT = 6,
	POINT_COUNT = 9,

	MODE_JOYSTICK = 0,
	MODE_MANUAL = 1,
	MODE_POINTS = 2,
	MODE_HOME = 3,

	KEY_JOYSTICK = 10,
	KEY_MANUAL = 11,
	KEY_POINTS = 12,
	KEY_HOME = 13,
	KEY_ADD = 14,
	KEY_SUB = 15,
	KEY_NONE = 255,

	KEY_LONG_PRESS = 50,
	MANUAL_STEP = 10,

	POINTS_MODE_MIN = 10,
	POINTS_MODE_MAX = 32;

const uint16_t
	MAX_ANALOG = 1023,
	MIN_ANALOG = 0,
	MAX_AXIS = 255,
	MIN_AXIS = 0;

LiquidCrystal_I2C lcd(0x27, 16, 2);	// I2C Adress, Width, Height

uint8_t
	curPoint[AXIS_COUNT],
	points[POINT_COUNT * AXIS_COUNT],
	curMode,
	curKey,
	curKeyTime,
	waitingTime,
	manualMode,
	pointsMode;
	
bool
	joystickMode;

void setup()
{
	PORTB &= 0xF0; DDRB |= 0x0F;	// (50 - 53) Outputs [Adress:  A0 - A3]
	PORTG &= 0xF8; DDRG |= 0x07;	// (39 - 41) Outputs [Control: CLK, RW, A15]
	PORTD &= 0x7F; DDRD |= 0x80;	// (38) 	 Outputs [Control: CS]
	PORTC  = 0x0F; DDRC  = 0xF0;	// (30 - 33) Inputs Pullups (34 - 37) Outputs [Keyboard]
	
    lcd.init();
    lcd.backlight();
	PrintAbout();
	
	for (uint8_t i = 0; i < POINT_COUNT * AXIS_COUNT; i++)
		points[i] = EEPROM.read(i);
	
	curMode = KEY_HOME;
	MoveTo(HOME_POINT);
}

void loop()
{
	ScanKey();
	
	if (curKey >= KEY_JOYSTICK && curKey <= KEY_HOME)
	{
		curMode = curKey - KEY_JOYSTICK;
		lcd.clear();
		lcd.setCursor(0, 0);

		switch (curKey)
		{
			case KEY_JOYSTICK:
				lcd.print(F("Joystick"));
				joystickMode = false;
				waitingTime = 100;
			break;
			case KEY_MANUAL:
				lcd.print(F("Manual"));
				manualMode = AXIS_COUNT;
				waitingTime = 100;
			break;
			case KEY_POINTS:
				lcd.print(F("Points"));
				pointsMode = POINT_COUNT;
			break;
			case KEY_HOME:
				lcd.print(F("Home"));
				MoveTo(HOME_POINT);
				waitingTime = 100;
			break;
		}
		
		lcd.print(F(" Mode"));
		curKeyTime = KEY_LONG_PRESS;
	}

	switch (curMode)
	{
		case MODE_JOYSTICK:
			GetJoystick(curPoint);
			MoveTo(curPoint);
			if (waitingTime)
			{
				if (--waitingTime == 0) lcd.clear();
			}
			else
			{
				if (curKeyTime < KEY_LONG_PRESS)
				{
					if (curKey == 0)
					{
						joystickMode = !joystickMode;
						lcd.clear();
						lcd.setCursor(0, 0);
						lcd.print(F("Point of:"));
						lcd.setCursor(0, 1);
						if (joystickMode)	lcd.print(F("Robot"));
						else				lcd.print(F("Joystick"));
						waitingTime = 100;
					}
					else CheckSavePoint();
					curKeyTime = KEY_LONG_PRESS;
				}
				else
				{
					if (joystickMode) GetArm(curPoint);
					PrintPoint(curPoint);
				}
			}
		break;
		case MODE_MANUAL:
			GetArm(curPoint);
			if (waitingTime)
			{
				if (--waitingTime == 0) lcd.clear();
			}
			else
			{
				if (curKeyTime < KEY_LONG_PRESS)
				{
					if (curKey == 0)
					{
						if (++manualMode > AXIS_COUNT)
						{
							manualMode = 0;
							MoveTo(curPoint);
						}
						lcd.clear();
						curKeyTime = KEY_LONG_PRESS;
					}
					else if (curKey == KEY_ADD)
					{
						if ((manualMode < AXIS_COUNT) && ((curKeyTime == 0) || (curKeyTime > 20)))
						{
							if (curPoint[manualMode] < MAX_AXIS - MANUAL_STEP) curPoint[manualMode] += MANUAL_STEP;
							else curPoint[manualMode] = MAX_AXIS;
							Write(manualMode, curPoint[manualMode]);
							curKeyTime = KEY_LONG_PRESS;
						}
					}
					else if (curKey == KEY_SUB)
					{
						if ((manualMode < AXIS_COUNT) && ((curKeyTime == 0) || (curKeyTime > 20)))
						{
							if (curPoint[manualMode] > MIN_AXIS + MANUAL_STEP) curPoint[manualMode] -= MANUAL_STEP;
							else curPoint[manualMode] = MIN_AXIS;
							Write(manualMode, curPoint[manualMode]);
							curKeyTime = KEY_LONG_PRESS;
						}
					}
					else if (CheckSavePoint()) curKeyTime = KEY_LONG_PRESS;
				}
				else
				{
					if (manualMode == AXIS_COUNT)
					{
						PrintPoint(curPoint);				
					}
					else
					{
						lcd.setCursor(0, 0);
						switch (manualMode)
						{
							case 0: lcd.print(F("Base"));		break;
							case 1: lcd.print(F("Shoulder"));	break;
							case 2: lcd.print(F("Elbow"));		break;
							case 3: lcd.print(F("Left"));		break;
							case 4: lcd.print(F("Right"));		break;
							case 5: lcd.print(F("Gripper"));	break;
						}
						lcd.print(F(": "));
						lcd.setCursor(0, 1);
						Print3Digits(curPoint[manualMode]);
						if (curKey == KEY_ADD || curKey == KEY_SUB) curKeyTime = 0;
					}
				}
			}
		break;
		case MODE_POINTS:
			if (waitingTime)
			{
				waitingTime--;
			}
			else if (curKeyTime < KEY_LONG_PRESS)
			{
				if (curKey == 0)
				{
					if (pointsMode < POINTS_MODE_MIN || pointsMode > POINTS_MODE_MAX)
					{
						lcd.clear();
						lcd.setCursor(0, 0);
						lcd.print(F("Test"));
						pointsMode = POINTS_MODE_MIN;
					}
					else pointsMode++;

					memcpy(curPoint, TEST_POINT, AXIS_COUNT);
					
					if (pointsMode & 0x01)
						switch (pointsMode - POINTS_MODE_MIN)
						{
							case 1:	 curPoint[0] = 0xFF;	break;
							case 3:  curPoint[0] = 0x00;	break;
							case 5:  curPoint[1] = 0xFF;	break;
							case 7:  curPoint[1] = 0x00;	break;
							case 9:  curPoint[2] = 0xFF;	break;
							case 11: curPoint[2] = 0x00;	break;
							case 13: curPoint[3] = 0x00;	break;
							case 15: curPoint[3] = 0xFF;	break;
							case 17: curPoint[4] = 0x00;	break;
							case 19: curPoint[4] = 0xFF;	break;
							case 21: curPoint[5] = 0x10;	break;
							case 23: curPoint[5] = 0x70;	break;
						}
						
					MoveTo(curPoint);
				}
				else if (curKey >= 1 && curKey <= 9)
				{
					pointsMode = curKey - 1;
					GoToPoint();
				}
				else if (curKey == KEY_ADD)
				{
					if (++pointsMode >= POINT_COUNT) pointsMode = 0;
					GoToPoint();
				}
				else if (curKey == KEY_SUB)
				{
					if (--pointsMode >= POINT_COUNT) pointsMode = POINT_COUNT - 1;
					GoToPoint();
				}

				curKeyTime = KEY_LONG_PRESS;
			}
		break;
		case MODE_HOME:
			if (waitingTime) if (--waitingTime == 0) PrintAbout();
		break;
	}
	
	if (curKey != KEY_NONE) if (curKeyTime < KEY_LONG_PRESS) curKeyTime++;
	delay(5);
}

void Write(uint8_t addres, uint8_t data)
{
	DDRL	= 0xFF;					// (43 - 49) Outputs [Data]
	PORTL	= data;
	PORTB	= (PORTB & 0xF0) | (addres & 0x0F);
	PORTG |= 0x01; PORTG &= 0xFE;	// Clock Pulse
}

void GoToPoint()
{
	if (pointsMode >= POINT_COUNT) return;
	memcpy(curPoint, points + pointsMode * AXIS_COUNT, AXIS_COUNT);
	MoveTo(curPoint);
	
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print(F("Going To"));
	lcd.setCursor(0, 1);
	lcd.print(F("Point_"));
	lcd.print(pointsMode + 1, DEC);
	lcd.print(F("!"));
}

void MoveTo(const uint8_t AXIS[])
{
	memcpy(curPoint, AXIS, AXIS_COUNT);
	MoveTo(curPoint);	
}
void MoveTo(uint8_t axis[])
{
	for (uint8_t i = 0; i < AXIS_COUNT; i++)
		Write(i, axis[i]);
}

void GetJoystick(uint8_t point[])
{
	for (uint8_t i = 0; i < AXIS_COUNT; i++)
		point[i] = map(analogRead(A0 + i), MIN_ANALOG, MAX_ANALOG, MIN_AXES[i], MAX_AXES[i]);
}

void GetArm(uint8_t point[])
{
	for (uint8_t i = 0; i < AXIS_COUNT; i++)
		point[i] = map(analogRead(A8 + i), MIN_ANALOG, MAX_ANALOG, MIN_AXIS, MAX_AXIS);
}

void PrintPoint(uint8_t point[])
{
	for (uint8_t i = 0; i < AXIS_COUNT; i++)
	{
		lcd.setCursor((i % 3) * 6, i / 3);
		Print3Digits(point[i]);
	}
}

void Print3Digits(uint8_t num)
{
	if (num < 100)	lcd.print('0');
	if (num < 10)	lcd.print('0');
	lcd.print(num);
}

void PrintAbout()
{
	lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print(F("Mentor  2014"));
	lcd.setCursor(1, 1);
	lcd.print(F("By Arad Eizen!"));
}

bool CheckSavePoint()
{
	if ((curKey < 1) || (curKey > 9)) return false;
	uint8_t pos = AXIS_COUNT * --curKey;
	
	memcpy(points + pos, curPoint, AXIS_COUNT);
	
	for (uint8_t i = 0; i < AXIS_COUNT; i++)
		EEPROM.write(i + pos, points[i + pos]);
	
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print(F("Position Saved"));
	lcd.setCursor(0, 1);
	lcd.print(F("as Point_"));
	lcd.print(++curKey, DEC);
	lcd.print(F("!"));
	waitingTime = 150;
	return true;
}

void ScanKey()
{
	uint8_t oldKey = curKey;
	curKey = KEY_NONE;

	for (uint8_t i = 0; i < 4; i++)
	{
		PORTC = (0xEF << i) | 0x0F;
		delay(5);
		switch (PINC & PORTC)
		{
			case 0x77: curKey = 1;				break; // 1
			case 0x7B: curKey = 2;				break; // 2
			case 0x7D: curKey = 3;				break; // 3
			case 0x7E: curKey = KEY_JOYSTICK;	break; // A
			case 0xB7: curKey = 4;				break; // 4
			case 0xBB: curKey = 5;				break; // 5
			case 0xBD: curKey = 6;				break; // 6
			case 0xBE: curKey = KEY_MANUAL;		break; // B
			case 0xD7: curKey = 7;				break; // 7
			case 0xDB: curKey = 8;				break; // 8
			case 0xDD: curKey = 9;				break; // 9
			case 0xDE: curKey = KEY_POINTS;		break; // C
			case 0xE7: curKey = KEY_SUB;		break; // *
			case 0xEB: curKey = 0;				break; // 0
			case 0xED: curKey = KEY_ADD;		break; // #
			case 0xEE: curKey = KEY_HOME;		break; // D
		}
		if (oldKey != curKey)
		{
			curKeyTime = 0;
			return;
		}
	}
	if (curKey == KEY_NONE) curKeyTime = KEY_LONG_PRESS;
}













