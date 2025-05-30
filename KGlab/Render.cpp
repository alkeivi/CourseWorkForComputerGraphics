#include "Render.h"
#include <Windows.h>
#include <GL\GL.h>
#include <GL\GLU.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include "GUItextRectangle.h"





#ifdef _DEBUG
#include <Debugapi.h> 
struct debug_print
{
	template<class C>
	debug_print& operator<<(const C& a)
	{
		OutputDebugStringA((std::stringstream() << a).str().c_str());
		return *this;
	}
} debout;
#else
struct debug_print
{
	template<class C>
	debug_print& operator<<(const C& a)
	{
		return *this;
	}
} debout;
#endif

//библиотека для разгрузки изображений
//https://github.com/nothings/stb
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

//внутренняя логика "движка"
#include "MyOGL.h"
extern OpenGL gl;
#include "Light.h"
Light light;
#include "Camera.h"
Camera camera;


bool texturing = true;
bool lightning = true;
bool alpha = false;


//переключение режимов освещения, текстурирования, альфаналожения
void switchModes(OpenGL *sender, KeyEventArg arg)
{
	//конвертируем код клавиши в букву
	auto key = LOWORD(MapVirtualKeyA(arg.key, MAPVK_VK_TO_CHAR));

	switch (key)
	{
	case 'L':
		lightning = !lightning;
		break;
	case 'T':
		texturing = !texturing;
		break;
	case 'A':
		alpha = !alpha;
		break;
	}
}

//Текстовый прямоугольничек в верхнем правом углу.
//OGL не предоставляет возможности для хранения текста
//внутри этого класса создается картинка с текстом (через виндовый GDI),
//в виде текстуры накладывается на прямоугольник и рисуется на экране.
//Это самый простой способ что то написать на экране
//но ооооочень не оптимальный
GuiTextRectangle text;

//айдишник для текстуры
GLuint texId;
//выполняется один раз перед первым рендером
void initRender()
{
	//==============НАСТРОЙКА ТЕКСТУР================
	//4 байта на хранение пикселя
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

	//просим сгенерировать нам Id для текстуры
	//и положить его в texId
	glGenTextures(1, &texId);

	//делаем текущую текстуру активной
	//все, что ниже будет применено texId текстуре.
	glBindTexture(GL_TEXTURE_2D, texId);


	int x, y, n;

	//загружаем картинку
	//см. #include "stb_image.h" 
	unsigned char* data = stbi_load("texture.png", &x, &y, &n, 4);
	//x - ширина изображения
	//y - высота изображения
	//n - количество каналов
	//4 - нужное нам количество каналов
	//пиксели будут хранится в памяти [R-G-B-A]-[R-G-B-A]-[..... 
	// по 4 байта на пиксель - по байту на канал
	//пустые каналы будут равны 255

	//Картинка хранится в памяти перевернутой 
	//так как ее начало в левом верхнем углу
	//по этому мы ее переворачиваем -
	//меняем первую строку с последней,
	//вторую с предпоследней, и.т.д.
	unsigned char* _tmp = new unsigned char[x * 4]; //времянка
	for (int i = 0; i < y / 2; ++i)
	{
		std::memcpy(_tmp, data + i * x * 4, x * 4);//переносим строку i в времянку
		std::memcpy(data + i * x * 4, data + (y - 1 - i) * x * 4, x * 4); //(y-1-i)я строка -> iя строка
		std::memcpy(data + (y - 1 - i) * x * 4, _tmp, x * 4); //времянка -> (y-1-i)я строка
	}
	delete[] _tmp;


	//загрузка изображения в видеопамять
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	//выгрузка изображения из опперативной памяти
	stbi_image_free(data);


	//настройка режима наложения текстур
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
												  //GL_REPLACE -- полная замена политога текстурой
	//настройка тайлинга
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	//настройка фильтрации
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//======================================================

	//================НАСТРОЙКА КАМЕРЫ======================
	camera.caclulateCameraPos();

	//привязываем камеру к событиям "движка"
	gl.WheelEvent.reaction(&camera, &Camera::Zoom);
	gl.MouseMovieEvent.reaction(&camera, &Camera::MouseMovie);
	gl.MouseLeaveEvent.reaction(&camera, &Camera::MouseLeave);
	gl.MouseLdownEvent.reaction(&camera, &Camera::MouseStartDrag);
	gl.MouseLupEvent.reaction(&camera, &Camera::MouseStopDrag);
	//==============НАСТРОЙКА СВЕТА===========================
	//привязываем свет к событиям "движка"
	gl.MouseMovieEvent.reaction(&light, &Light::MoveLight);
	gl.KeyDownEvent.reaction(&light, &Light::StartDrug);
	gl.KeyUpEvent.reaction(&light, &Light::StopDrug);
	//========================================================
	//====================Прочее==============================
	gl.KeyDownEvent.reaction(switchModes);
	text.setSize(512, 180);
	//========================================================
	   

	camera.setPosition(2, 1.5, 1.5);
}

void CalculateQuadNormal(const double A[3], const double B[3], const double C[3], const double D[3], double N[3], bool invertZ = false) {
	// Вектора в плоскости четырёхугольника
	double AB[3] = { B[0] - A[0], B[1] - A[1], B[2] - A[2] };
	double AD[3] = { D[0] - A[0], D[1] - A[1], D[2] - A[2] };

	// Векторное произведение AD × AB (меняем порядок, чтобы нормаль была "наружу")
	N[0] = AD[1] * AB[2] - AD[2] * AB[1];
	N[1] = -AD[0] * AB[2] + AD[2] * AB[0];
	N[2] = AD[0] * AB[1] - AD[1] * AB[0];

	// Нормализация
	double length = sqrt(N[0] * N[0] + N[1] * N[1] + N[2] * N[2]);
	if (length > 0) {
		N[0] /= length;
		N[1] /= length;
		N[2] /= length;
	}

	// Инверсия по Z при необходимости
	if (invertZ) {
		N[2] = -N[2];
	}
}

void CalculateQuadCenter(const double A[3], const double B[3], const double C[3], const double D[3], double center[3]) {
	center[0] = (A[0] + B[0] + C[0] + D[0]) / 4.0;
	center[1] = (A[1] + B[1] + C[1] + D[1]) / 4.0;
	center[2] = (A[2] + B[2] + C[2] + D[2]) / 4.0;
}

void DrawNormal(const double center[3], const double normal[3], const double color[3]) {
	double normal_end[3] = {
		center[0] + normal[0],
		center[1] + normal[1],
		center[2] + normal[2]
	};

	bool b_light = glIsEnabled(GL_LIGHTING);
	// Отключаем освещение, чтобы раскрасить вектор привычным нам glColor
	if (b_light)
		glDisable(GL_LIGHTING);

	glBegin(GL_LINES);
	glColor3dv(color);
	glVertex3dv(center);
	glVertex3dv(normal_end);
	glEnd();

	// Восстанавливаем освещение, если нужно
	if (b_light)
		glEnable(GL_LIGHTING);
}

void Render(double delta_time)
{    
	glEnable(GL_DEPTH_TEST);
	
	//натройка камеры и света
	//в этих функциях находятся OGLные функции
	//которые устанавливают параметры источника света
	//и моделвью матрицу, связанные с камерой.

	if (gl.isKeyPressed('F')) //если нажата F - свет из камеры
	{
		light.SetPosition(camera.x(), camera.y(), camera.z());
	}
	camera.SetUpCamera();
	light.SetUpLight();


	//рисуем оси
	gl.DrawAxes();

	glDisable(GL_LIGHTING);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	

	//включаем режимы, в зависимости от нажания клавиш. см void switchModes(OpenGL *sender, KeyEventArg arg)
	if (lightning)
		glEnable(GL_LIGHTING);
	if (texturing)
	{
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0); //сбрасываем текущую текстуру
	}
		
	if (alpha)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
		
	//=============НАСТРОЙКА МАТЕРИАЛА==============


	//настройка материала, все что рисуется ниже будет иметь этот метериал.
	//массивы с настройками материала
	float  amb[] = { 0.2, 0.2, 0.1, 1. };
	float dif[] = { 0.4, 0.65, 0.5, 1. };
	float spec[] = { 0.9, 0.8, 0.3, 1. };
	float sh = 0.2f * 256;

	//фоновая
	glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
	//дифузная
	glMaterialfv(GL_FRONT, GL_DIFFUSE, dif);
	//зеркальная
	glMaterialfv(GL_FRONT, GL_SPECULAR, spec); 
	//размер блика
	glMaterialf(GL_FRONT, GL_SHININESS, sh);

	//чтоб было красиво, без квадратиков (сглаживание освещения)
	glShadeModel(GL_SMOOTH); //закраска по Гуро      
			   //(GL_SMOOTH - плоская закраска)

	//============ РИСОВАТЬ ТУТ ==============

	//...

	double A[]{ 0, 2, 0 };
	double B[]{ 4, 4, 0 };
	double C[]{ 1, 0, 0 };
	double D[]{ 7, -6, 0 };
	double E[]{ -2, -9, 0 };
	double F[]{ -7, -4, 0 };
	double G[]{ -1, 0, 0 };
	double H[]{ -5, 7, 0 };
	double A1[]{ 0, 2, 2 };
	double B1[]{ 4, 4, 2 };
	double C1[]{ 1, 0, 2 };
	double D1[]{ 7, -6, 2 };
	double E1[]{ -2, -9, 2 };
	double F1[]{ -7, -4, 2 };
	double G1[]{ -1, 0, 2 };
	double H1[]{ -5, 7, 2 };

	double N[3];
	double center[3];
	double normalColor[]{ 1, 0, 0 };

	//БОКА
	CalculateQuadNormal(A, B, B1, A1, N);
	CalculateQuadCenter(A, B, B1, A1, center);
	DrawNormal(center, N, normalColor);
	glBegin(GL_QUADS);
	glNormal3dv(N);
	glColor4d(0.8, 0.9, 0.9, 1);
	glVertex3dv(A);
	glVertex3dv(A1);
	glVertex3dv(B1);
	glVertex3dv(B);
	glEnd();

	CalculateQuadNormal(B, C, C1, B1, N);
	CalculateQuadCenter(B, C, C1, B1, center);
	DrawNormal(center, N, normalColor);
	glBegin(GL_QUADS);
	glNormal3dv(N);
	glColor4d(0.3, 0.3, 0.4, 1);
	glVertex3dv(C);
	glVertex3dv(B);
	glVertex3dv(B1);
	glVertex3dv(C1);
	glEnd();

	CalculateQuadNormal(C, D, D1, C1, N);
	CalculateQuadCenter(C, D, D1, C1, center);
	DrawNormal(center, N, normalColor);
	glBegin(GL_QUADS);
	glNormal3dv(N);
	glColor4d(0.4, 0.2, 0.9, 1);
	glVertex3dv(C);
	glVertex3dv(C1);
	glVertex3dv(D1);
	glVertex3dv(D);
	glEnd();

	CalculateQuadNormal(D, E, E1, D1, N);
	CalculateQuadCenter(D, E, E1, D1, center);
	DrawNormal(center, N, normalColor);
	glBegin(GL_QUADS);
	glNormal3dv(N);
	glColor4d(0.1, 0.2, 0.6, 1);
	glVertex3dv(D);
	glVertex3dv(D1);
	glVertex3dv(E1);
	glVertex3dv(E);
	glEnd();

	CalculateQuadNormal(E, F, F1, E1, N);
	CalculateQuadCenter(E, F, F1, E1, center);
	DrawNormal(center, N, normalColor);
	glBegin(GL_QUADS);
	glNormal3dv(N);
	glColor4d(0, 0.9, 0.9, 1);
	glVertex3dv(E);
	glVertex3dv(E1);
	glVertex3dv(F1);
	glVertex3dv(F);
	glEnd();

	CalculateQuadNormal(F, G, G1, F1, N);
	CalculateQuadCenter(F, G, G1, F1, center);
	DrawNormal(center, N, normalColor);
	glBegin(GL_QUADS);
	glNormal3dv(N);
	glColor4d(0.1, 0.5, 0.5, 1);
	glVertex3dv(F);
	glVertex3dv(F1);
	glVertex3dv(G1);
	glVertex3dv(G);
	glEnd();

	CalculateQuadNormal(G, H, H1, G1, N);
	CalculateQuadCenter(G, H, H1, G1, center);
	DrawNormal(center, N, normalColor);
	glBegin(GL_QUADS);
	glNormal3dv(N);
	glColor4d(0.2, 0.6, 0.9, 1);
	glVertex3dv(G);
	glVertex3dv(G1);
	glVertex3dv(H1);
	glVertex3dv(H);
	glEnd();

	CalculateQuadNormal(H, A, A1, H1, N);
	CalculateQuadCenter(A, H, H1, A1, center);
	DrawNormal(center, N, normalColor);
	glBegin(GL_QUADS);
	glNormal3dv(N);
	glColor4d(0.3, 0.3, 0.4, 1);
	glVertex3dv(H);
	glVertex3dv(H1);
	glVertex3dv(A1);
	glVertex3dv(A);
	glEnd();

	//НИЗ
	glNormal3d(0, 0, -1);

	glBegin(GL_QUADS);
	glColor4d(0.9, 0.9, 0.9, 1);
	glVertex3dv(A);
	glVertex3dv(B);
	glVertex3dv(C);
	glVertex3dv(G);
	glEnd();

	glBegin(GL_QUADS);
	glColor4d(0.8, 0.8, 0.8, 1);
	glVertex3dv(G);
	glVertex3dv(C);
	glVertex3dv(E);
	glVertex3dv(F);
	glEnd();

	glBegin(GL_TRIANGLES);
	glColor4d(0.6, 0.6, 0.6, 1);
	glVertex3dv(C);
	glVertex3dv(D);
	glVertex3dv(E);
	glEnd();

	glBegin(GL_TRIANGLES);
	glColor4d(0.5, 0.5, 0.5, 1);
	glVertex3dv(H);
	glVertex3dv(A);
	glVertex3dv(G);
	glEnd();

	//ВЕРХ
	glNormal3d(0, 0, 1);

	glBindTexture(GL_TEXTURE_2D, texId);
	glBegin(GL_QUADS);
	glColor4d(0.4, 0.4, 0.7, 0.35);
	glTexCoord2d(0.5, 0.6875);
	glVertex3dv(A1);
	glTexCoord2d(0.75, 0.8125);
	glVertex3dv(B1);
	glTexCoord2d(0.5625, 0.5625);
	glVertex3dv(C1);
	glTexCoord2d(0.4375, 0.5625);
	glVertex3dv(G1);
	glEnd();
	glBindTexture(GL_TEXTURE_2D, 0);

	glBindTexture(GL_TEXTURE_2D, texId);
	glBegin(GL_QUADS);
	glColor4d(0.3, 0.6, 0.7, 0.35);
	glTexCoord2d(0.4375, 0.5625);
	glVertex3dv(G1);
	glTexCoord2d(0.5625, 0.5625);
	glVertex3dv(C1);
	glTexCoord2d(0.375, 0);
	glVertex3dv(E1);
	glTexCoord2d(0.0625, 0.3125);
	glVertex3dv(F1);
	glEnd();
	glBindTexture(GL_TEXTURE_2D, 0);

	glBindTexture(GL_TEXTURE_2D, texId);
	glBegin(GL_TRIANGLES);
	glColor4d(0.7, 0.8, 0.6, 0.35);
	glTexCoord2d(0.1875, 1);
	glVertex3dv(H1);
	glTexCoord2d(0.5, 0.6875);
	glVertex3dv(A1);
	glTexCoord2d(0.4375, 0.5625);
	glVertex3dv(G1);
	glEnd();
	glBindTexture(GL_TEXTURE_2D, texId);

	glBindTexture(GL_TEXTURE_2D, texId);
	glBegin(GL_TRIANGLES);
	glColor4d(0.6, 0.8, 0.7, 0.35);
	glTexCoord2d(0.5625, 0.5625);
	glVertex3dv(C1);
	glTexCoord2d(0.9375, 0.1875);
	glVertex3dv(D1);
	glTexCoord2d(0.375, 0);
	glVertex3dv(E1);
	glEnd();
	glBindTexture(GL_TEXTURE_2D, 0);


	
	//===============================================

	//рисуем источник света
	light.DrawLightGizmo();

	//================Сообщение в верхнем левом углу=======================

	//переключаемся на матрицу проекции
	glMatrixMode(GL_PROJECTION);
	//сохраняем текущую матрицу проекции с перспективным преобразованием
	glPushMatrix();
	//загружаем единичную матрицу в матрицу проекции
	glLoadIdentity();

	//устанавливаем матрицу паралельной проекции
	glOrtho(0, gl.getWidth() - 1, 0, gl.getHeight() - 1, 0, 1);

	//переключаемся на моделвью матрицу
	glMatrixMode(GL_MODELVIEW);
	//сохраняем матрицу
	glPushMatrix();
    //сбразываем все трансформации и настройки камеры загрузкой единичной матрицы
	glLoadIdentity();

	//отрисованное тут будет визуалзироватся в 2д системе координат
	//нижний левый угол окна - точка (0,0)
	//верхний правый угол (ширина_окна - 1, высота_окна - 1)

	
	std::wstringstream ss;
	ss << std::fixed << std::setprecision(3);
	ss << "T - " << (texturing ? L"[вкл]выкл  " : L" вкл[выкл] ") << L"текстур" << std::endl;
	ss << "L - " << (lightning ? L"[вкл]выкл  " : L" вкл[выкл] ") << L"освещение" << std::endl;
	ss << "A - " << (alpha ? L"[вкл]выкл  " : L" вкл[выкл] ") << L"альфа-наложение" << std::endl;
	ss << L"F - Свет из камеры" << std::endl;
	ss << L"G - двигать свет по горизонтали" << std::endl;
	ss << L"G+ЛКМ двигать свет по вертекали" << std::endl;
	ss << L"Коорд. света: (" << std::setw(7) <<  light.x() << "," << std::setw(7) << light.y() << "," << std::setw(7) << light.z() << ")" << std::endl;
	ss << L"Коорд. камеры: (" << std::setw(7) << camera.x() << "," << std::setw(7) << camera.y() << "," << std::setw(7) << camera.z() << ")" << std::endl;
	ss << L"Параметры камеры: R=" << std::setw(7) << camera.distance() << ",fi1=" << std::setw(7) << camera.fi1() << ",fi2=" << std::setw(7) << camera.fi2() << std::endl;
	ss << L"delta_time: " << std::setprecision(5)<< delta_time << std::endl;

	
	text.setPosition(10, gl.getHeight() - 10 - 180);
	text.setText(ss.str().c_str());
	text.Draw();

	//восстанавливаем матрицу проекции на перспективу, которую сохраняли ранее.
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	

}   



