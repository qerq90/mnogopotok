#include <windows.h>
#include <stdio.h>
#include <wincon.h>
#include <stdlib.h>

#define MAIN_WINDOW_HEIGHT 40
#define MAIN_WINDOW_WIDTH 99 
#define WINDOWS_NUMBER 3 
#define WINDOW_WIDTH MAIN_WINDOW_WIDTH / WINDOWS_NUMBER 
#define WINDOW_HEIGHT 5
#define NUMBER_OF_VISITERS 24 
#define TERMINAL_WORKING_TIME 3 * 1000 
#define X_POS_FOR_TERMINAL (MAIN_WINDOW_WIDTH/2 - 10) 
#define Y_POS_FOR_TERMINAL (MAIN_WINDOW_HEIGHT/2 - 2)
#define TIME_OF_SLEEP 100 

typedef struct // структура для работы с потоком
{
	char id;
	int time;
	int x;
	int y;
	int in_building;
} ThreadArgs;

typedef struct { // структура для работы с окном
	int id;
	int time;
	char visiter;
} WindowInfo;

typedef struct { // структура для работы с терминалом
	int last_ticket_id;
	char visiter;
} TicketInfo;

void cls_scr(); // Очистка экрана
void write_many(char, int, int, int); // функция для записи одного символа несколько раз в определенной точке
void make_window(int, int, int, int); // функция для создания окна
void refresh_window_info(int); // функция для обновления данных модели согласно данным структуры окна
void write(char *, int, int); // функция для записи строки в определенной точке
void setCursor(int, int); // функция для перемещения курсора
void start_model(); // функция для создания модели
void show_model(); // функция для отображения модели
void change_window_info(int, int, int, char); // функция для изменения значений в окне
int change_ticket_info(char); // функция для получения талона и изменения значений в терминале
void refresh_ticket_window_info(); // функция для обновления данных модели согласно данным структуры терминала
void refresh_model(); // функция для обновления данных модели согласно данным потоков(расположение букв)
void get_to(ThreadArgs *, int, int); // функция передвижения посетителя в определенную точку
void change_position(ThreadArgs *, int, int); // функция для изменения положения посетителя на 1 единицу
int check(int, int); // функция проверки возможности хода
DWORD WINAPI ThreadVisiter(LPVOID); // основная функция создаваемого потока,представляющая из себя посетителя

WindowInfo WindInfo[WINDOWS_NUMBER]; // массив структур окон
TicketInfo TickInfo; // структура терминала

ThreadArgs * Params[NUMBER_OF_VISITERS]; // массив структур аргументов потоков
HANDLE ThreadVisiters[NUMBER_OF_VISITERS]; // массив дескрипторов потоков
HANDLE WriteMutex; // дескриптор мьютекса,отвечающего за вывод на экран
HANDLE TerminalMutex; // дескриптор мьютекса,отвечающего за контроль терминала
HANDLE WindowMutexes[WINDOWS_NUMBER]; // Контроль доступа к окнам

int main() 
{
	for(int i = 0;i < NUMBER_OF_VISITERS;i++) // заполняем массив указателей на структуры для каждого посетителя
	{
		ThreadArgs * test = (ThreadArgs *)malloc(sizeof(ThreadArgs)); // выделяем память на heap для каждой структуры
		test->id = i + 'A'; // id имеет представление [A-Z],поэтому просто будем прибавлять i к 'A'
		test->time = (rand() % 270 + 30) * 350; // генерируем от 0,5 до 5 минут (переводим в миллисекунды)
		test->x = MAIN_WINDOW_WIDTH/4 + i*2; // задаем координаты для "места" каждого посетителя
		test->y = MAIN_WINDOW_HEIGHT - 3;
		test->in_building = 0;
		Params[i] = test;
	}

	start_model();
	show_model();

	WriteMutex = CreateMutex(NULL, 0, NULL); // стандартное создание мьютекса

	if(WriteMutex == NULL) // стандартная проверка на успех создания мьютекса
	{
		printf("Failed to make a Write mutex");
		return 1;
	}

	TerminalMutex = CreateMutex(NULL, 0, NULL);

	if(TerminalMutex == NULL) 
	{
		printf("Failed to make a Terminal mutex");
		return 1;
	}

	for(int i = 0;i < WINDOWS_NUMBER;i++)
	{
		WindowMutexes[i] = CreateMutex(NULL, 0, NULL);

		if(WindowMutexes[i] == NULL)
		{
			printf("Failed to make a terminal mutex");
			return 1;
		}
	}

	for(int i = 0;i < NUMBER_OF_VISITERS;i++) // создаем все потоки
	{
		int * id = malloc(sizeof(int));
		*id = i;
		ThreadVisiters[i] = CreateThread(NULL, 0, ThreadVisiter, id, 0, 0); 

		if(ThreadVisiters[i] == NULL)
		{
			printf("Failed to make a Thread");
			return 1;
		}
	}

	WaitForMultipleObjects(NUMBER_OF_VISITERS, ThreadVisiters, 1, INFINITE); // ждем окончания действия всех потоков
	setCursor(0,0); // возвращаем курсор в позицию 0 0

	for(int i = 0;i < NUMBER_OF_VISITERS;i++) // дальше просто закрываем все возможные дескрипторы
	{
		CloseHandle(ThreadVisiters[i]); 
	}

	for(int i = 0;i < NUMBER_OF_VISITERS;i++) // освобождаем выделенную память
	{
		free(Params[i]);
	}

	for(int i = 0;i < WINDOWS_NUMBER;i++) 
	{
		CloseHandle(WindowMutexes[i]);
	}

	CloseHandle(WriteMutex);
	CloseHandle(TerminalMutex);

	getchar(); // ждем любого действия пользователя,дабы модель сразу не очищалась
	cls_scr(); // очищаем экран

	return 0;
}

int get_length(char * string) // получение длины строки(только для терминированной строки)
{
	int i = 0;
	while(*(string + i) != '\0')
	{
		i++;
	}

	return i;
}

void setCursor(int x, int y)
{
	HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE); // здесь мы получаем дескриптор нашей консоли,чтобы наши действия относились к ней

	COORD pos;
	pos.X = x;
	pos.Y = y;

	SetConsoleCursorPosition(hout, pos);
}

void write(char * string, int x, int y) // только терминированные строки
{
	int len = get_length(string);
	DWORD written;
	HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);

	setCursor(x , y);

	WriteConsole(hout, string, len, &written, NULL);
}

void write_many(char symbol, int x, int y, int num)
{
	HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
	COORD pos;
	DWORD written;

	pos.X = x;
	pos.Y = y;
	
	FillConsoleOutputCharacter(hout, symbol, (WORD) num, pos, &written);
}

void make_window(int x, int y, int width, int height)
{
	write_many('-',x,y,width);
	write_many('-',x,y+height,width);

	for(int i = y+1;i < y+height;i++)
	{
		write("|", x, i);
		write("|", x+width-1, i);
	}
}

void start_model()
{
	CONSOLE_CURSOR_INFO * info = malloc(sizeof(CONSOLE_CURSOR_INFO)); // создаем структуру info, чтобы наш курсор был невидимым
	info->dwSize = 1;
	info->bVisible = 0;
	HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);

	SetConsoleCursorInfo(hout, info);
	free(info);

	for(int i = 0; i < WINDOWS_NUMBER; i++) // ставим стандартные значения для окон
	{
		change_window_info(i, 0, 0, 0);
	}

	TickInfo.last_ticket_id = 0; //ставим стандартные значения для терминала
	TickInfo.visiter = 0;

}

void refresh_ticket_window_info()
{
	char* ticket_string = (char *) malloc(sizeof(char) * 30);
	char* visiter_string = (char *) malloc(sizeof(char) * 30);

	sprintf(ticket_string, "Last ticket id %d", TickInfo.last_ticket_id);
	sprintf(visiter_string, "Visiter id %c", TickInfo.visiter);

	write(ticket_string, X_POS_FOR_TERMINAL+1 , Y_POS_FOR_TERMINAL+1);
	write(visiter_string, X_POS_FOR_TERMINAL+1 , Y_POS_FOR_TERMINAL+2);

	free(ticket_string);
	free(visiter_string);
}

void refresh_window_info(int window_id)
{
	WindowInfo * info = &WindInfo[window_id];
	char* ticket_string = (char *) malloc(sizeof(char) * 30);
	char* visiter_string = (char *) malloc(sizeof(char) * 30);
	char* time_string = (char *) malloc(sizeof(char) * 30);

	sprintf(ticket_string, "Ticket id - %d", info->id);
	sprintf(time_string, "Time - %dc", info->time);
	sprintf(visiter_string, "Visiter - %c", info->visiter);

	write(ticket_string, 3 + window_id*WINDOW_WIDTH, 3);
	write(time_string, 3 + window_id*WINDOW_WIDTH, 4);
	write(visiter_string, 3 + window_id*WINDOW_WIDTH, 5);

	free(ticket_string);
	free(time_string);
	free(visiter_string);
}

void change_window_info(int window_id, int ticket_id, int time, char visiter)
{
	WindowInfo * info = &WindInfo[window_id]; // получаем нужное окно
	info->id = ticket_id;
	info->time = time;
	info->visiter = visiter;
}

int change_ticket_info(char visiter)
{
	TickInfo.visiter = visiter;
	int ticket_id = ++TickInfo.last_ticket_id;
	return ticket_id;
}

void show_model()
{
	cls_scr();
	make_window(2, 2, MAIN_WINDOW_WIDTH, MAIN_WINDOW_HEIGHT); //главное окно модели
	
	for(int i = 0; i < WINDOWS_NUMBER; i++) // 3 окна приема
	{
		make_window(2 + i*WINDOW_WIDTH, 2, WINDOW_WIDTH, WINDOW_HEIGHT);
		refresh_window_info(i);
	}

	make_window(X_POS_FOR_TERMINAL, Y_POS_FOR_TERMINAL, 20, 5); // терминал
	refresh_ticket_window_info();
	
	setCursor(0, 0);
}

void refresh_model()
{
	for(int i = 0; i < WINDOWS_NUMBER; i++)
	{
		refresh_window_info(i);
	}

	refresh_ticket_window_info();

	for(int i = 0; i < NUMBER_OF_VISITERS; i++) // смотрим,есть ли посетитель в здании и если он есть - отрисовываем его
	{
		if(Params[i]->in_building)
		{
			char * id = malloc(sizeof(char));
			sprintf(id, "%c", Params[i]->id);

			write(id, Params[i]->x, Params[i]->y);
			free(id);
		}
	}
}

int check(int x, int y)
{
	for(int i = 0; i < NUMBER_OF_VISITERS; i++)
	{
		if(Params[i]->in_building && Params[i]->x == x && Params[i]->y == y)
		{
			return 0;
		}
	}

	return 1;
}

void change_position(ThreadArgs * params, int x, int y)
{
	if(check(x, y)) {
		WaitForSingleObject(WriteMutex, INFINITE);

		write(" ", params->x, params->y);
		params->x = x;
		params->y = y;
		refresh_model();

		ReleaseMutex(WriteMutex);
	}
}

void get_to(ThreadArgs * params, int x, int y)
{
	while(1)
	{
		Sleep(TIME_OF_SLEEP);

		if(x != params->x || y != params->y)
		{	
			if(x < params->x) 
			{
				change_position(params, params->x - 1, params->y);
				continue;
			}
			
			if(x > params->x) 
			{
				change_position(params, params->x + 1, params->y);
				continue;
			}

			if(y > params->y) 
			{
				change_position(params, params->x, params->y + 1);
				continue;
			}

			if(y < params->y) {
				change_position(params, params->x, params->y - 1);
				continue;
			}
		}

		break;
	}
}

DWORD WINAPI ThreadVisiter(LPVOID Thread_id) // здесь все также,как в прошлой версии.Саму логику действий потока я не менял,менялось лишь отображение.
{
	int * idptr = (int *) Thread_id; // явно задаем тип 
	int id = *idptr; // удобнее работать с переменной,а не указателем,поэтому просто задаем id
	srand(Params[id]->time); // каждому потоку задаем свое состояние генератора радомных чисел

	Sleep(Params[id]->time); // условно говоря,во время этого сна мы ждем пока посетитель дойдет до почты
	WaitForSingleObject(WriteMutex, INFINITE); // получаем разрешение на то,чтобы менять что-то в модели
	Params[id]->in_building = 1; // меняем флаг,ведь мы уже находимся в здании
	refresh_model(); // обновляем модель(в этот момент нарисуется наша буква)
	ReleaseMutex(WriteMutex); // освобождаем мьютекс для других потоков

	get_to(Params[id], X_POS_FOR_TERMINAL + 10, Y_POS_FOR_TERMINAL + 6); // Добираемся до терминала

	WaitForSingleObject(TerminalMutex, INFINITE); // ждем разрешение на то,чтобы использовать терминал
	int ticket_id = change_ticket_info(Params[id]->id); // получаем номер билетика
	
	WaitForSingleObject(WriteMutex, INFINITE); // ждем разрешение на изменение модели
	refresh_ticket_window_info(); // обновляем информацию в окне терминала
	ReleaseMutex(WriteMutex); // освобождаем для других потоков

	Sleep(TERMINAL_WORKING_TIME); // условно получаем билет
	ReleaseMutex(TerminalMutex); // освобождаем терминал для других посетителей

	get_to(Params[id], MAIN_WINDOW_WIDTH/4 + id*2, MAIN_WINDOW_HEIGHT - 3); // доходим обратно до своего места в зале

	WaitForSingleObject(WriteMutex, INFINITE); // ждем разрешение на изменение модели
	Params[id]->in_building = 0; // убираем этот флаг,чтобы другие потоки могли проходить сквозь нас
	ReleaseMutex(WriteMutex); // освобождаем для других потоков

	int index = WaitForMultipleObjects(WINDOWS_NUMBER, WindowMutexes, 0, INFINITE) - WAIT_OBJECT_0; // ждем,пока одно из окон освободится
	WaitForSingleObject(WriteMutex, INFINITE); // ждем разрешение на изменение модели
	Params[id]->in_building = 1; // не забываем вернуть флаг в положение "в здании"
	ReleaseMutex(WriteMutex); // освобождаем для других потоков
	int time = (rand() % 25 + 36) * 1000; // генерируем время работы окна

	WaitForSingleObject(WriteMutex, INFINITE); // ждем разрешение на изменение модели
	change_window_info(index, ticket_id, time / 1000, Params[id]->id); // изменяем информацию в окне
	write(" ", Params[id]->x, Params[id]->y); // телепортация к окну в следующих 3 строчках
	Params[id]->x = WINDOW_WIDTH/2 + WINDOW_WIDTH*index;
	Params[id]->y = 4 + WINDOW_HEIGHT;
	refresh_model(); // обновляем модель
	ReleaseMutex(WriteMutex); // освобождаем для других потоков

	Sleep(time); // условно работаем с окном
	ReleaseMutex(WindowMutexes[index]); // освобождаем окно для другого посетителя

	WaitForSingleObject(WriteMutex, INFINITE); // ждем разрешение на изменение модели
	write(" ", Params[id]->x, Params[id]->y); // телепортация обратно на свое место в следующих 3 строчках
	Params[id]->x = MAIN_WINDOW_WIDTH/4 + id*2;
	Params[id]->y = MAIN_WINDOW_HEIGHT - 3;
	refresh_model(); // обновляем модель
	ReleaseMutex(WriteMutex); // освобождаем для других потоков
	Sleep(1000); // ждем секунду,чтобы было видно,как посетитель уходит с почты

	WaitForSingleObject(WriteMutex, INFINITE); // ждем разрешение на изменение модели
	Params[id]->in_building = 0; // выход с почты
	write(" ", Params[id]->x, Params[id]->y); // убираем с модели наше представление 
	ReleaseMutex(WriteMutex); // освобождаем для других потоков

	ExitThread(0); // конец потока
}

void cls_scr()
{
	system("cls");
}