#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <Windows.h>
#include <conio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

HANDLE port;
DCB com_port;
FILE *input_msg, *output_msg;

void last_msg(char *str);
char command1();
void delete_the_key();
void add_new_key();
void encrypt_message();
void decrypt_message();
void generate_signature();
void check_signature();
void sent_a_and_p(); 
void sent_int(int a);
char get_confirmation(char *success_msg);
char calculate_hash();

int main()
{
	// открыть порт
	port = CreateFileA("COM8", GENERIC_READ | GENERIC_WRITE, NULL, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (port == INVALID_HANDLE_VALUE)
	{
		last_msg("COM-port connection is failed.\n");
		return 0;
	}
	else printf("COM-port connection is established.\n");

	// установить параметры com_port
	com_port.DCBlength = sizeof(com_port);
	if (GetCommState(port, &com_port) == 0)
		last_msg("COM-port state error.\n");
	com_port.BaudRate = CBR_2400;
	com_port.Parity = NOPARITY;
	com_port.ByteSize = 8;
	com_port.StopBits = ONESTOPBIT;
	if (SetCommState(port, &com_port) == 0)
		last_msg("COM-port state error.\n");

	// начинаем взаимодействие с пользователем
	char command_number;
	while (1)
	{
		command_number = command1();
		switch (command_number)
		{
		case 1:
			delete_the_key();
			break;
		case 2:
			add_new_key();
			break;
		case 3:
			encrypt_message();
			break;
		case 4:
			decrypt_message();
			break;
		case 5:
			generate_signature();
			break;
		case 6:
			check_signature();
			break;
		}
	}
	return 0;
}

void delete_the_key()
{
	// узнать у пользователя, какой ключ он хочет удалить
	printf("Print A and P parameteres of the key you want to delete (use space to separate parameteres).\n");

	// отправить МК значения а и р
	sent_a_and_p();

	get_confirmation("The key was successfully deleted.\n");
}

void add_new_key()
{
	// узнать у пользователя а, р, х
	printf("Print A, P and X parameteres of new key (use space to separate parameteres).\n");

	// отправить параметры МК
	sent_a_and_p();
	int x;
	scanf("%d", &x);
	sent_int(x);

	// получить подтверждение
	get_confirmation("New key was added successfully.\n");
	// считываем \n
	char tmp;
	scanf("%c", &tmp);
}

void encrypt_message()
{
	// узнать у пользователя файл с сообщением
	printf("Print the input file name.\n");
	char *name = (char*)malloc(100 * sizeof(char));
	scanf("%s", name);
	while ((input_msg = fopen(name, "r")) == 0)
		printf("Error. Try again.\n");
	// узнать у пользователя имя файла, куда записать зашифрованное сообщение
	printf("Print the crypt file name (the encrypted message would be written in there.\n");
	scanf("%s", name);
	while ((output_msg = fopen(name, "w")) == 0)
		printf("Error. Try again.\n");
	// узнать у пользователя параметры а и р, и отправить МК
	printf("Print A and P parameteres of encryption key (use space to separate parameteres).\n");
	sent_a_and_p();

	// посчитать длину сообщения и отправить МК	
	int length;
	fseek(input_msg, 0, SEEK_END);
	length = ftell(input_msg);
	fseek(input_msg, 0, SEEK_SET);
	sent_int(length);

	// получить подтверждение
	if (get_confirmation("The key was founded. Message was encrypt.\n") == 'я')
	{
		fclose(input_msg);
		fclose(output_msg);

		// считываем \n
		char tmp;
		scanf("%c", &tmp);
		return;
	}
	// получить a^y и сообщить его пользователю
	int a_pow_y = 0;
	int buffer = 0;
	ReadFile(port, &buffer, 1, NULL, NULL);
	a_pow_y = buffer;
	a_pow_y <<= 8;
	ReadFile(port, &buffer, 1, NULL, NULL);
	a_pow_y += buffer;
	printf("A^Y = %d.\n", a_pow_y);

	// отправить сообщение
	int i; 
	for (i = 0; i < length; i++)
	{
		// считать байт шифруемого соощения, отправить его МК
		buffer = fgetc(input_msg);
		WriteFile(port, &buffer, 1, NULL, NULL);
		Sleep(10);
		// считать присланый МК байт, записать в output_msg
		ReadFile(port, &buffer, 1, NULL, NULL);
		fputc(buffer, output_msg);
	}
	fclose(input_msg);
	fclose(output_msg);
	
	// считываем \n
	char tmp;
	scanf("%c", &tmp);
}

void decrypt_message()
{
	// узнать у пользователя файл с зашифрованным сообщением
	printf("Print the input file name.\n");
	char *name = (char*)malloc(100*sizeof(char));
	scanf("%s", name);
	input_msg = fopen(name, "r");
	// узнать у пользователя имя файла, куда записать расшифрованное сообщение
	printf("Print the decrypt file name (the decrypted message would be written in there.\n");
	scanf("%s", name);
	output_msg = fopen(name, "w");

	// узнать у пользователя а, р, a^y
	printf("Print A, P and A^Y parameteres of new key (use space to separate parameteres).\n");
	// отправить параметры МК
	sent_a_and_p();
	int a_pow_y;
	scanf("%d", &a_pow_y);
	sent_int(a_pow_y);

	// посчитать длину сообщения и отправить МК	
	int length;
	fseek(input_msg, 0, SEEK_END);
	length = ftell(input_msg);
	fseek(input_msg, 0, SEEK_SET);
	sent_int(length);

	// получить подтверждение
	if (get_confirmation("The key was founded. Message was decrypt.\n") > 1)
		return;

	// отправить сообщение
	int i;
	uint8_t buffer;
	for (i = 0; i < length; i++)
	{
		// считать байт зашифрованного соощения, отправить его МК
		buffer = fgetc(input_msg);
		WriteFile(port, &buffer, 1, NULL, NULL);
		Sleep(10);
		// считать присланый МК байт, записать в output_msg
		ReadFile(port, &buffer, 1, NULL, NULL);
		fputc(buffer, output_msg);
	}
	fclose(input_msg);
	fclose(output_msg);
	// считываем \n
	char tmp;
	scanf("%c", &tmp);
}

void generate_signature()
{
	// узнать у пользователя, по какому ключу он хочет вычислить подпись
	printf("Print A and P parameteres of the key you want to generate a signature (use space to separate parameteres).\n");
	sent_a_and_p();

	// узнать у пользователя, по какому сообщению он хочет вычислить подпись
	printf("Print the input file name.\n");
	char *name = (char*)malloc(100 * sizeof(char));
	scanf("%s", name);
	input_msg = fopen(name, "r");

	// отправить а и р, вычислить хэш сообщения и отправить его
	char hash = calculate_hash();
	WriteFile(port, &hash, 1, NULL, NULL);
	Sleep(10);

	// получить подтверждение
	if (get_confirmation("The signature was generated successfully.\n") > 1)
		return;
	// получить подписи
	int s1, s2;
	int buffer;
	ReadFile(port, &buffer, 1, NULL, NULL);
	s1 = buffer;
	s1 <<= 8;
	ReadFile(port, &buffer, 1, NULL, NULL);
	s1 += buffer;
	ReadFile(port, &buffer, 1, NULL, NULL);
	s2 = buffer;
	s2 <<= 8;
	ReadFile(port, &buffer, 1, NULL, NULL);
	s2 += buffer;
	printf("The signatures: s1 = %d; s2 = %d.\n", s1, s2);
	// считываем \n
	char tmp;
	scanf("%c", &tmp);
}

void check_signature()
{
	// узнать у пользователя, по какому ключу он хочет вычислить подпись
	printf("Print A and P parameteres of the key you want to check the signature (use space to separate parameteres).\n");
	sent_a_and_p();

	// узнать у пользователя, по какому сообщению он хочет вычислить подпись
	printf("Print the input file name.\n");
	char *name = (char*)malloc(100 * sizeof(char));
	scanf("%s", name);
	input_msg = fopen(name, "r");

	// отправить а и р, вычислить хэш сообщения и отправить его
	char hash = calculate_hash();
	WriteFile(port, &hash, 1, NULL, NULL);
	Sleep(10);

	// узнать у пользователя подписи для проверки
	printf("Print signatures S1 and S2 to check (use space to separate parameteres).\n");
	sent_a_and_p(); 

	// получить подтверждение
	char buffer;
	ReadFile(port, &buffer, 1, NULL, NULL);
	if (buffer == 1)
		printf("The signatures are correct.\n");
	else if (buffer == 0x0F)
		printf("The signatures are not correct.\n");
	else
		printf("Error.\n");
	// считываем \n
	char tmp;
	scanf("%c", &tmp);
}

void sent_a_and_p()
{
	// отправить а
	int a;
	scanf("%d", &a);
	sent_int(a);

	// отправить р
	scanf("%d", &a);
	sent_int(a);
}

void sent_int(int a)
{
	char buffer = a >> 8;
	WriteFile(port, &buffer, 1, NULL, NULL);
	Sleep(100);
	buffer = a;
	WriteFile(port, &buffer, 1, NULL, NULL);
	Sleep(100);
}

char get_confirmation(char *success_msg)
{
	// получить подтверждение
	char buffer;
	ReadFile(port, &buffer, 1, NULL, NULL);

	// вывести пользователю результат
	if (buffer == 1)
		printf(success_msg);
	else
		printf("Error.\n");
	return buffer;
}

void last_msg(char *str)
{
	printf(str);
	_getch();
}

char command1()
{
	printf("Choose the command:\n\
			1 - delete the key;\n\
			2 - add new key;\n\
			3 - encrypt the message;\n\
			4 - decrypt the message;\n\
			5 - generate the signature;\n\
			6 - check the signature.\n");
	char c, tmp;
	scanf("%c", &c);
	// считываем \n
	scanf("%c", &tmp);
	c -= '0';
	WriteFile(port, &c, 1, NULL, NULL);
	Sleep(10);
	return c;
}

char calculate_hash()
{
	// посчитать длину сообщения	
	int length;
	fseek(input_msg, 0, SEEK_END);
	length = ftell(input_msg) + 1;
	fseek(input_msg, 0, SEEK_SET);

	// высчитать хэш и вернуть его
	char buffer, hash = 0;
	for (int i = 0; i < length; i++)
	{
		hash <<= 1;
		buffer = fgetc(input_msg);
		hash += buffer;
	}
	return hash;
}