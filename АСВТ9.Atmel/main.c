#define F_CPU 8000000
#define UBRR_C (F_CPU / (9600 * 16) - 1)
//#include <asf.h>
#include <math.h>
//#include <avr/iom32.h>
#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <util/delay.h>

char command_number,  // какой байт команды сейчас в обработке
request,       // номер запроса от ПК:
// 1 - удаление ключа
// 2 - добавление ключа
// 3 - шифрование сообщения
// 4 - дешифрование сообщения
// 5 - вычисление хэша и электронной подписи
// 6 - вычисление хэша и проверка электронной подписи
cond,            //  при работе с ключами: 1/3/5 -> пересылается первый байт а/р/х;
				 //                        2/4/6 -> пересылается второй байт
key_counter,
message_length,
m,               // при работе с шированием: текущий байт сообщения
output_mode,     // при выводе на семисегметный индикатор:
// 1 - вывод имени а
// 2 - вывод значения а на PA3
// 3 - вывод значения а на PA2
// 4 - вывод значения а на PA1
// 5 - вывод значения а на PA0
// 6 - вывод имени р
// 7 - вывод значения p на PA3
// 8 - вывод значения p на PA2
// 9 - вывод значения p на PA1
// 10 - вывод значения p на PA0
numbers[10] = {0b00111111,       // вывод чисел на семисигментные индикаторы
	0b00000110,
	0b01011011,
	0b01001111,
	0b01100110,
	0b01101101,
	0b01111101,
	0b00000111,
	0b01111111,
	0b01101111},
letter_a = 0b01011111,           // вывод букв на семисигментные индикаторы
letter_p = 0b01110011,
print_key_number,                // номер выводимого на индикаторы ключа
error_msg;                       // ответ мк после выполнения команды пк:
								// 1 = выполнено; 0xFF = ошибка

unsigned int
var_y, f, a, k,     // для шифрования и дешифровки сообщения
hash_, r,           // для подписи. hash - один байт!
var_delay,                         // для зарержки вывода на индикатор
work_key[3],					   // ключ (а, р, х)
print_key[2];                      // а и р, выводимые на семисегментный индикатор

unsigned long int s1, s2, v1, v2, a_pow_y, b_pow_y;

char uart_read()
{
	while(!(UCSRA&(1<<RXC))) ;
		return UDR;
}

void uart_write(char c)
{
	while (!(UCSRA&(1<<UDRE))) ;
	UDR = c;
}

void EEPROM_Write(unsigned int uiAddress, unsigned char ucData)
{
	/* Wait for completion of previous write */
	while(EECR & (1<<EEWE))
	;
	/* Set up address and data registers */
	EEAR = uiAddress;
	EEDR = ucData;
	/* Write logical one to EEMWE */
	EECR |= (1<<EEMWE);
	/* Start eeprom write by setting EEWE */
	EECR |= (1<<EEWE);
}

unsigned char EEPROM_Read(unsigned int uiAddress)
{
	/* Wait for completion of previous write */
	while(EECR & (1<<EEWE))
	;
	/* Set up address register */
	EEAR = uiAddress;
	/* Start eeprom read by writing EERE */
	EECR |= (1<<EERE);
	/* Return data from data register */
	return EEDR;
}

void key_element()  // все int передаются двумя словами (побайтно)
{
	if (cond == 1)
	{
		//work_key[0] = uart_read() - '0';
		work_key[0] = uart_read();
		work_key[0] =  work_key[0] << 8;
	}
	else  if (cond == 2)
	{
		char temp;
		//temp = uart_read() - '0';
		temp = uart_read();
		work_key[0] += temp;
	}
	else if (cond == 3)
	{
		work_key[1] = uart_read();
		//work_key[1] = uart_read() - '0';
		work_key[1] <<= 8;
	}
	else if (cond == 4)
	{
		char temp;
		//temp = uart_read() - '0';
		temp = uart_read();
		work_key[1] += temp;
	}
	else if (cond == 5)
	{
		//work_key[2] = uart_read() - '0';
		work_key[2] = uart_read();
		work_key[2] <<= 8;
	}
	else if (cond == 6)
	{
		char temp;
		//temp = uart_read() - '0';
		temp = uart_read();
		work_key[2] += temp;
	}
}

void sent_confirmation()
{
	uart_write(error_msg);
	_delay_ms(100);
	error_msg = 1;
}

int find_the_key()
{
	if (key_counter == 0)
		return -1;
	unsigned int i = 0, a = 0, p = 0;
	// ищем нужный ключ
	while(1)
	{
		// ищем а
		while(1)
		{
			a = EEPROM_Read(i*6);
			a <<= 8;
			a += EEPROM_Read(i*6+1);
			if (a == work_key[0])
				break;
			i++;
		}
		// если пара (а, р) правильная, выходим из цикла поиска
		p = EEPROM_Read(i*6+2);
		p <<= 8;
		p += EEPROM_Read(i*6+3);
		if (p == work_key[1])
			break;
		i++;
		if (i > key_counter)
			return -1;
	}
	work_key[2] = EEPROM_Read(i*6+4);
	work_key[2] <<= 8;
	work_key[2] += EEPROM_Read(i*6+5);
	return i;
}

void load_the_key()
{
	print_key[0] = EEPROM_Read(print_key_number*6);
	print_key[0] <<= 8;
	print_key[0] += EEPROM_Read(print_key_number*6+1);
	print_key[1] =  EEPROM_Read(print_key_number*6+2);
	print_key[1] <<= 8;
	print_key[1] += EEPROM_Read(print_key_number*6+3);
}

void write_the_key()
{
	EEPROM_Write(key_counter*6,   work_key[0]>>8);
	EEPROM_Write(key_counter*6+1, work_key[0]);
	EEPROM_Write(key_counter*6+2, work_key[1]>>8);
	EEPROM_Write(key_counter*6+3, work_key[1]);
	EEPROM_Write(key_counter*6+4, work_key[2]>>8);
	EEPROM_Write(key_counter*6+5, work_key[2]);
	key_counter++;
	if (key_counter == 1)
		load_the_key();
	command_number = 0;
	sent_confirmation();
}

int prepare_the_key()
{
	a_pow_y = work_key[2];
	return find_the_key();
}

void sent_back_int(unsigned int a)
{
	uart_write(a>>8);
	_delay_ms(100);
	uart_write(a);
	_delay_ms(100);
}

void sent_back_before_encrypt()
{
	long temp, i, j;
	if (prepare_the_key() < 0)
	{
		error_msg = 0xFF;
		command_number = 0;
		sent_confirmation();
		return;
	}
	sent_confirmation();
	message_length = a_pow_y; // костыль, чтоб можно было использовать prepare_the_key()
	// выбираем случайное var_y

	var_y = rand() % (work_key[1]-3) + 1;
	// вычисляем a^var_y и b^var_y mod p

	i = 0;
	a_pow_y = 1;
	while (i < var_y) {
		a_pow_y = a_pow_y * work_key[0];
		a_pow_y = a_pow_y % work_key[1];
		i++;

	}


	b_pow_y = 1;
	j = 0;
	while (j < work_key[2]) {
		b_pow_y = b_pow_y * a_pow_y;
		b_pow_y = b_pow_y % work_key[1];
		j++;
	}
	// передаем ПК a^y mod p
	sent_back_int(a_pow_y);
}

void encrypt_message()  //f = (m + b^y) mod p
{
	//m = uart_read() - '0';
	m = uart_read();
	f = (char)((m + b_pow_y) % work_key[1]);
	//f += '0';
	uart_write(f);
	_delay_ms(10);
	if (command_number == message_length + 7)
		command_number = 0;
}

void decrypt_message()  //m = (f - b^y) mod p
{
	long temp, i;
	//f = uart_read() - '0';
	f = uart_read();
	
		/*
		temp = 1;
		i = work_key[2];
		while (i > 0)
		{
			temp *= a_pow_y;
			temp %= work_key[1];
			i--;
		}
		b_pow_y = temp;
		*/

	b_pow_y = 1;
	i = 0;
	while (i < work_key[2]) {
		b_pow_y = b_pow_y * a_pow_y;
		b_pow_y = b_pow_y % work_key[1];
		i++;
	}
	
	long f_ = f, b_pow_y_ = b_pow_y;
	m = (char)((f_ - b_pow_y_) % work_key[1]); 

	uart_write(m);
	_delay_ms(10);
	if (command_number == message_length + 9)
		command_number = 0;
}

void decrypt_help1()
{
	int i;
	a_pow_y = work_key[2];
	i = prepare_the_key();
	if (i < 0)
		error_msg = 0x05;
	//else
	//    a_pow_y = work_key[2];
	work_key[2] = 0;
}

void decrypt_help2()
{
	int i;
	message_length = work_key[2];
	i = find_the_key();
	if (error_msg > 1 || i < 0)
	{
		error_msg = 0xFF;
		command_number = 0;
	}
	sent_confirmation();
}

int gcd (int a, int b)
{
	if (b == 0)
	return a;
	else
	return gcd (b, a % b);
}

void calculate_signature()
{
	command_number = 0;
	long temp;
	int i;
	i = find_the_key();
	if (i < 0)
	{
		error_msg = 0xFF;
		command_number = 0;
		sent_confirmation();
		return;
	}
	sent_confirmation();
	
	// сгенерировать случайное число r
	r = rand() % (work_key[1] - 3) + 2;
	while ((work_key[1] - 1) % r == 0)
		r = rand() % (work_key[1] - 3) + 2;
	//sent_back_int(r);
	// вычислить s1 = a^r mod p
	temp = 1;
	i = r;
	while (i > 0)
	{
		temp *= work_key[0];
		temp %= work_key[1];
		i--;
	}
	s1 = temp;
	// передать s1
	sent_back_int(s1);
	
	// вычислить s2 = (hash_ - x*s1)*(r^(-1)) mod (p-1)
	i = 1;
	if (work_key[2]*s1 > hash_)
	{
		temp = work_key[2]*s1 - hash_;
		i = -1;
	}
	else
	temp = hash_ - work_key[2]*s1;
	temp %= work_key[1] - 1;
	if (i == -1)
	temp = work_key[1] - 1 - temp;
	int inv_r = 1;
	while (gcd(r, inv_r) != 1 || inv_r*r%(work_key[1] - 1) != 1)
	{
		inv_r++;
		if (inv_r == work_key[1] - 1)
		{
			inv_r = 1;
			break;
		}
	}
	//sent_back_int(inv_r);
	s2 = (temp * inv_r) % (work_key[1] - 1);
	// передать s2
	sent_back_int(s2);
}

void check_signature()
{
	long temp;
	int i;
	command_number = 0;
	if (find_the_key() < 0)
	{
		error_msg = 0xFF;
		command_number = 0;
		sent_confirmation();
		return;
	}
	// вычисляем v1 = a^hash_ mod p
	temp = 1;
	i = hash_;
	while (i > 0)
	{
		temp *= work_key[0];
		temp %= work_key[1];
		i--;
	}
	v1 = temp;
	//sent_back_int(v1);
	// вычисляем v2 = (a^x)^s1 * s1^s2 mod p
	temp = 1;
	i = work_key[2] * s1;
	while (i > 0)
	{
		temp *= work_key[0];
		temp %= work_key[1];
		i--;
	}
	v2 = temp;
	temp = 1;
	i = s2;
	while (i > 0)
	{
		temp *= s1;
		temp %= work_key[1];
		i--;
	}
	v2 *= temp;
	v2 %= work_key[1];
	//sent_back_int(v2);
	// если v1 == v2, подпись верна, отвправляем ПК единицу, иначе 0x0F
	if (v1 - v2 == 0)
	error_msg = 1;
	else
	error_msg = 0x0F;
	sent_confirmation();
}

void clean()
{
	work_key[0] = 0;
	work_key[1] = 0;
	work_key[2] = 0;
}

void delete_the_key()
{
	// удалить ключ:
	// найти нужный ключ
	int i, j;
	i = find_the_key();
	if (key_counter == 0 || i < 0)
	{
		command_number = 0;
		error_msg = 0xFF;
		sent_confirmation();
		return;
	}
	// заменить на 0xFF
	EEPROM_Write(i*6,0xFF);
	EEPROM_Write(i*6+1,0xFF);
	EEPROM_Write(i*6+2,0xFF);
	EEPROM_Write(i*6+3,0xFF);
	EEPROM_Write(i*6+4,0xFF);
	EEPROM_Write(i*6+5,0xFF);

	// сдвинуть все ключи ниже
	i *= 6;
	for (j = i; j < key_counter*6; j++)
	{
		a = EEPROM_Read(j+6);
		EEPROM_Write(j, a);
	}
	command_number = 0;
	sent_confirmation();
}

void full_buffer()
{
	if (error_msg == 0xFF)
		return;
	command_number++;
	
	if (command_number > 7 && request == 3)
	{
		encrypt_message();
		return;
	}
	if (command_number > 9 && request == 4)
	{
		decrypt_message();
		return;
	}
	
	switch (command_number)
	{
		case 1:     // запрос
		//request = uart_read() - '0';
		request = uart_read();
		clean();
		break;
		case 2:     // получаем первый байт a
		cond = 1;
		key_element();
		break;
		case 3:     // получаем второй байт a
		cond = 2;
		key_element();
		break;
		case 4:     // получаем первый байт р
		cond = 3;
		key_element();
		break;
		case 5:     // получаем второй байт p
		cond = 4;
		key_element();
		if (request == 1)
			delete_the_key();
		break;
		case 6:     // при работе в ключами (добавление) - получаем первый байт х
		// при работе с шифрованием - получаем первый байт длины будущего сообщения
		// при работе с дешифровкой - получаем первый байт a^y
		// при работе с подписью - получаем байт хэша
		cond = 5;
		key_element();
		if (request > 4)
		{
			work_key[2] >>= 8;
			hash_ = work_key[2];
			if (request == 5)
				calculate_signature();
		}
		break;
		case 7:     // при работе в ключами (добавление) - получаем второй байт х
		// при работе с шифрованием - получаем второй байт длины будущего сообщения
		// при работе с дешифровкой - получаем второй байт a^y
		// при работе с проверкой подписи - получаем первый байт s1
		if (request < 6)
		{
			cond = 6;
			key_element();
			if (request == 2)
				write_the_key();
			else if (request == 3)
				sent_back_before_encrypt();
			else if (request == 4)
				decrypt_help1();
			/*else if (request == 5)
			{
				if (prepare_the_key() < 0)
					error_msg = 0xFF;
				message_length = a_pow_y;  // костыль, чтоб можно было пользоваться функцией сверху
			}*/
		}
		else
		{
			cond = 5;
			key_element();
		}
		break;
		case 8:        // при работе с дешифровкой - получаем первый байт длины будущего сообщения
		// при работе с проверкой подписи - получаем второй байт s1
		if (request == 6)
		{
			cond = 6;
			key_element();
			s1 = work_key[2];
			work_key[2] = 0;
		}
		else if (request == 4)
		{
			cond = 5;
			key_element();
		}
		break;
		case 9:        // при работе с дешифровкой - получаем второй байт длины будущего сообщения
		// при работе с проверкой подписи - получаем первый байт s2
		if (request == 6)
		{
			cond = 5;
			key_element();
		}
		else if (request == 4)
		{
			cond = 6;
			key_element();
			decrypt_help2();
		}
		break;
		case 10:       // при работе с проверкой подписи - получаем второй байт s2
		cond = 6;
		key_element();
		s2 = work_key[2];
		check_signature();
		break;
	}
}

void print_on_port()
{
	unsigned int temp;
	switch (output_mode)
	{
		case 1:
		PORTA = 1;
		PORTC = letter_a;
		break;
		case 2:
		PORTA = 0b00001000;
		temp = print_key[0] / 1000;
		PORTC = numbers[temp];
		break;
		case 3:
		PORTA = 0b00000100;
		temp = print_key[0] / 100;
		temp %= 10;
		PORTC = numbers[temp];
		break;
		case 4:
		PORTA = 0b00000010;
		temp = print_key[0] / 10;
		temp %= 10;
		PORTC = numbers[temp];
		break;
		case 5:
		PORTA = 1;
		temp = print_key[0];
		temp %= 10;
		PORTC = numbers[temp];
		break;
		case 6:
		PORTA = 1;
		PORTC = letter_p;
		break;
		case 7:
		PORTA = 0b00001000;
		temp = print_key[1] / 1000;
		PORTC = numbers[temp];
		break;
		case 8:
		PORTA = 0b00000100;
		temp = print_key[1] / 100;
		temp %= 10;
		PORTC = numbers[temp];
		break;
		case 9:
		PORTA = 0b00000010;
		temp = print_key[1] / 10;
		temp %= 100;
		PORTC = numbers[temp];
		break;
		case 10:
		PORTA = 1;
		temp = print_key[1];
		temp %= 10;
		PORTC = numbers[temp];
		break;
	}
}

void change_print_key()
{
	if (key_counter < 2)
		return;
	print_key_number++;
	print_key_number %= key_counter;
	load_the_key();
	output_mode = 1;
	//var_delay = 4000;
	print_on_port();
}

void change_print_letter()
{
	output_mode += 5;
	output_mode %= 10;
	//var_delay = 4000;
	print_on_port();
}

void change_print_on_port()
{
	var_delay--;

	if (output_mode ==  1 || output_mode == 6)
	{
		// задержать на секунду
		if (var_delay > 0)
		return;
		
		output_mode++;
	}
	else
	{
		// var_delay на каждый индикатор == 10,
		// т. е. каждый десятый тик выводим новое число на индикатор;
		if (var_delay % 10 == 0)
		{
			output_mode++;
			if (output_mode == 6)
				output_mode = 2;
			else if (output_mode == 11)
				output_mode = 7;
			print_on_port();
		}
		if (var_delay > 0)
			return;
		if (output_mode < 6)
			output_mode = 1;
		else
			output_mode = 6;
	}
	var_delay = 4000;
	print_on_port();
}

ISR(USART_RXC_vect)
{
// костыль, чтоб понять, срабатывает ли это прерывание
/*output_mode = 6;
var_delay = 10000;
print_on_port();*/
//
	full_buffer();
	// 
}

ISR(INT0_vect)
{
	change_print_key();
	 
}

ISR(INT1_vect)
{
	change_print_letter();
	 
}

ISR(TIMER0_OVF_vect)
{
	change_print_on_port();
	// 
}

int main()
{
	DDRA=0xFF;
	DDRB=0xFF;
	DDRC=0xFF;
	//DDRD=0b11110010;
	DDRD=0b11110000;

	PORTA=0b00000001;
	PORTB=0;
	PORTC=letter_a;
	PORTD=0;

	// работа с таймером
	TIFR=0b00000001;
	TCCR0=0b00000010;  // последние 3 - коэффициент предделителя
	TIMSK=0b00000001;  // разрешение прерываний по таймеру0
	//TIFR = (1<<TOV0);

	// разрешение прерываний
	MCUCR=0b00001111;
	GICR=0b11000000;

	//UART init
	UBRRH = 0b10000000;
	UBRRL = 0b11001111;
	UCSRB = (1<<RXCIE)|(1<<RXEN)|(1<<TXEN); // Биты RXCIE(7), TXCIE(6), UDRIE(5) разрешеют прерывания по завершению приема, передачи и опустошению буфера
	UCSRC = (1<<UCSZ0)|(1<<UCSZ1)|(1<<URSEL);

	// тело программы
	command_number = 0;
	request = 100;
	cond = 0;

	key_counter = 0;
	message_length = 0;
	m = 0;
	var_y = 0;
	f = 0;
	a = 0;
	k = 0;
	a_pow_y = 0;
	b_pow_y = 0;
	output_mode = 1;
	print_key_number = 0;
	var_delay = 4000;
	error_msg = 1;
	
	sei();
	while (1)
	{ ; }
	return 0;
}


