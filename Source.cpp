#include <Windows.h>
#include <random>
#include <functional>
#include <ctime>
#include <list>
#include <algorithm>
#include "resource.h"

#include <CommCtrl.h>
#pragma comment(lib, "comctl32.lib")
#define BALLMAX 70
#define FrameRate 20
#define LifeLimit 10
#define BombLimit 10
#define PI (3.14159265358979)
#define MAXACCEL 5
class Entity;
class Ball;
class Self;
class Bullet;

ATOM RegisterCustomClass(HINSTANCE& hInstance);
LRESULT CALLBACK WndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam);
VOID CALLBACK Vulnerable(HWND hWnd, UINT iMsg, UINT_PTR idEvent, DWORD dwTime);
VOID CALLBACK Thaw(HWND hWnd, UINT iMsg, UINT_PTR idEvent, DWORD dwTime);
VOID CALLBACK DifficultyRise(HWND hWnd, UINT iMsg, UINT_PTR idEvent, DWORD dwTime);
VOID CALLBACK LineDrawProc(int x, int y, LPARAM lpData);
void EndGame(HWND hWnd, BOOL Clear);
void NewGame(HWND hWnd);

enum { ID_STATUS = 100 };
enum { FrameTimer, InvTimer, ThawTimer, DiffTimer };

LPCTSTR Title = L"PangPang";
HINSTANCE g_hInst;
HWND hMainWnd;
HWND hState;
HWND hProg;
HBITMAP MemBit;
HPEN TargetPointer;
HBRUSH BkBrush;
int DestPos;
int CurPos;
bool Frozen = false;
struct Vec2D
{
	double x, y;
	Vec2D(double x, double y) :x{ x }, y{ y }{};
	Vec2D() :x{ 0 }, y{ 0 }{};
	~Vec2D(){};
	Vec2D Rotate(double theta)
	{
		return Vec2D{ x*cos(theta) - y*sin(theta), x*sin(theta) + y*cos(theta) };
	}
	POINT toPoint(){ return{ x, y }; }
};
class Entity
{
	friend class Initializer;
	friend VOID CALLBACK DifficultyRise(HWND hWnd, UINT iMsg, UINT_PTR CallerID, DWORD dwTime);
	friend VOID CALLBACK LineDrawProc(int x, int y, LPARAM lpParam);
protected:
	double dx, dy;
	double ax, ay;
	int x, y;
	static std::function<int()> RandInt;
	static std::function<double()> RandDbl;
public:
	Entity() :x{ 0 }, y{ 0 }, ax{ 0 }, ay{ 0 }, dx{ 0 }, dy{ 0 }{};
	static int xLim, yLim;
	virtual void Move() = 0;
	virtual void Print(HDC hdc) = 0;
	virtual bool isPtOnMe(int ax, int ay) = 0;
	void Push(double Fx, double Fy){ ax += Fx, ay += Fy; }
	bool isEntityOnMe(Entity* E){ return isPtOnMe(E->x, E->y); }
};
int Entity::xLim;
int Entity::yLim;
std::function<int()> Entity::RandInt;
std::function<double()> Entity::RandDbl;

std::list<Ball*> Balls;
class Ball:public Entity
{
	friend class Initializer;
	HBRUSH myColorBrush;
	COLORREF Color;
	int size;
public:
	Ball() :Entity()
	{
		x = abs(RandInt()) % xLim;
		y = abs(RandInt()) % yLim;
		ax = RandDbl()*0.2;
		ay = RandDbl();
		size = 2 + abs(RandInt()) % 20;
		DestPos += size;
		Color = RGB(RandInt() % 255, RandInt() % 255, RandInt() % 255);
		myColorBrush = CreateSolidBrush(Color);
	}
	Ball(const Ball& other, double dx, double dy)
	{
		x = other.x;
		y = other.y;
		ax = other.ax;
		ay = other.ay;
		Color = other.Color;
		this->dx = dx;
		this->dy = dy;
		myColorBrush = CreateSolidBrush(Color);
		size = other.size / 2;
		DestPos += size;
	}
	~Ball(){ DeleteObject(myColorBrush); CurPos += size; };
	virtual void Move()
	{
		ax *= 0.9;
		ay *= 0.9; //friction...ish.
		dx += ax,dy += ay;
		
		dx *= 0.999;
		dy *= 0.999;

		x += dx, y += dy;
		//bounce
		if (x < 0)
		{
			x = -x;
			dx = abs(dx);
		}
		if (x > xLim)
		{
			x = xLim+xLim - x;
			dx = -abs(dx);
		}
		if (y < 0)
		{
			y = -y;
			dy = abs(dy);
		}
		if (y > yLim)
		{
			y = yLim+yLim - y;
			dy = -abs(dy);
		}
	};
	virtual void Print(HDC hdc){ SelectObject(hdc, myColorBrush);Ellipse(hdc, x - size, y - size, x + size, y + size); }
	virtual bool isPtOnMe(int ax, int ay)
	{
		if (abs(ax - x) > size || abs(ay - y) > size) return false;
		else if ((ax - x)*(ax - x) + (ay - y)*(ay - y) < size*size) return true;
		else return false;
	}
	int getSize(){ return size; };
	bool split()
	{
		if (size/2 > 4)
		{	
			Ball* ChildBallLeft = new Ball(*this, dy, -dx);
			Ball* ChildBallRight = new Ball(*this, -dy, dx);

			Balls.remove(this);
			delete this;

			Balls.push_front(ChildBallLeft);
			Balls.push_front(ChildBallRight);
			ChildBallLeft->Move();
			ChildBallRight->Move();
			return true;
		}
		else
		{
			Balls.remove(this);
			delete this;
			return false;
		}
	}
};

std::list<Bullet*> Bullets;
class Bullet :public Entity
{
	int size;
public:
	virtual void Move()
	{
		x += dx;
		y += dy;
	}
	virtual bool InBound()
	{
		return 0 < x&&x < xLim && 0 < y&&y < yLim;
	}
	virtual void Print(HDC hdc)
	{
		SelectObject(hdc, GetStockObject(BLACK_BRUSH));
		Rectangle(hdc, x - size, y - size, x + size, y + size);
	}
	virtual bool isPtOnMe(int x, int y)
	{
		return !(abs(ax - x) > size || abs(ay - y) > size);
	}
	Bullet(int x, int y, double dx, double dy, int size = 5) :Entity()
	{
		this->x = x;
		this->y = y;
		this->dx = dx;
		this->dy = dy;
		this->size = size;
	}
};

class Barrier : public Bullet
{
	int srcx, srcy;
	int radius;
	virtual bool InBound()
	{
		return radius*radius > (srcx - x)*(srcx - x) + (srcy - y)*(srcy - y);
	}
public:
	Barrier(int x, int y, double dx, double dy, int radius) :Bullet(x, y, dx, dy,2), srcx(x), srcy(y), radius(radius)
	{
	}
};


class Self:public Entity
{
	friend VOID CALLBACK Vulnerable(HWND, UINT, UINT_PTR, DWORD);
private:
	friend class Initializer;
	static HBITMAP myImage;
	static HBITMAP myInvincibleImage;
	static HBITMAP myMaskImage;
	static HIMAGELIST myAnimatedImage;
	static HIMAGELIST myAnimatedInvincibleImage;
	static int Width;
	static int Height;
	int Life;
	int NumBomb;
	int WalkState;
	double theta;
	bool Invincible;
public:
	bool Left, Right, Up, Down;
	Self() : Entity()
	{
		Life = LifeLimit;
		NumBomb = BombLimit;
		Left = false;
		Right = false;
		Up = false;
		Down = false;
		Invincible = true;
		this->x = xLim / 2;
		this->y = yLim - Height / 2;
	}
	virtual void Move()
	{
		ax *= 0.7;
		ay *= 0.7;
		dx /= 2;
		dy /= 2;

		
		if (Right || Left || Up || Down)
		{
			WalkState = (WalkState + 1) % 8;
			if (Right) ax += cos(theta + PI / 2), ay += sin(theta + PI / 2);
			if (Left) ax += cos(theta - PI / 2), ay += sin(theta - PI / 2);
			if (Up) ax += cos(theta), ay += sin(theta);
			if (Down) ax += cos(-theta), ay += sin(-theta);
		}
		else WalkState = 0;
 //friction...ish.
		dx += ax, dy += ay;
		x += dx, y += dy;

		if (x < 0)
		{
			x = -x;
			dx = abs(dx);
		}
		if (x > xLim)
		{
			x = xLim + xLim - x;
			dx = -abs(dx);
		}
		if (y < 0)
		{
			y = -y;
			dy = abs(dy);
		}
		if (y > yLim)
		{
			y = yLim + yLim - y;
			dy = -abs(dy);
		}
	};
	virtual void Print(HDC hdc)
	{
		HDC MemDC = CreateCompatibleDC(hdc);
		HBITMAP NewBit = CreateCompatibleBitmap(hdc, 48, 48);
		HBITMAP OldBit = (HBITMAP)SelectObject(MemDC, NewBit);
		//HBITMAP OldBit = (HBITMAP)SelectObject(MemDC, Invincible?myInvincibleImage:myImage);
		RECT R{ 0, 0, 48, 48 };
		FillRect(MemDC, &R, (HBRUSH)GetStockObject(Frozen?GRAY_BRUSH:WHITE_BRUSH));

		ImageList_Draw(Invincible ? myAnimatedInvincibleImage : myAnimatedImage, WalkState, MemDC, 0, 0, ILD_NORMAL);

		Vec2D MyRect[] = { Vec2D(-Width / 2, -Height / 2), Vec2D(Width / 2, -Height / 2), Vec2D(-Width / 2, Height / 2) };
		POINT LU = MyRect[0].Rotate(theta).toPoint();
		POINT RU = MyRect[1].Rotate(theta).toPoint();
		POINT LD = MyRect[2].Rotate(theta).toPoint();
		RU.x += x, RU.y += y;
		LU.x += x, LU.y += y;
		LD.x += x, LD.y += y;
		POINT RPts[] = { LU, RU, LD };
		PlgBlt(hdc, RPts, MemDC, 0, 0, Width, Height, NULL, NULL, NULL);
		HPEN TempPen = (HPEN)SelectObject(hdc, TargetPointer);

		Vec2D GunOrig(Width / 2, 7);
		POINT GP = GunOrig.Rotate(theta).toPoint();
		GP.x += x,GP.y += y;

		MoveToEx(hdc, GP.x, GP.y, NULL);
		Vec2D GunDest(100, 0);
		POINT GD = GunDest.Rotate(theta).toPoint();
		//LineTo(hdc,GP.x +  GD.x, GP.y + GD.y);
		double EllipseSize = 10;
		auto Param = std::make_pair (hdc, &EllipseSize) ;
		LineDDA(GP.x, GP.y, GP.x + GD.x, GP.y + GD.y, &LineDrawProc, (LPARAM)&Param);
		SelectObject(hdc, TempPen);
		TCHAR LifeText[30], BombText[30], ProgText[30];
		wsprintf(LifeText, L"Life : %d", Me->Life);
		wsprintf(BombText, L"Bomb : %d", Me->NumBomb);
		wsprintf(ProgText, L"Progress %d/%d", CurPos, DestPos);
		SendMessage(hState, SB_SETTEXT, 0, (LPARAM)LifeText);
		SendMessage(hState, SB_SETTEXT, 1, (LPARAM)BombText);
		SendMessage(hState, SB_SETTEXT, 2, (LPARAM)ProgText);
		SelectObject(MemDC, OldBit);
		DeleteObject(NewBit);
		DeleteObject(MemDC);
	};
	virtual bool isPtOnMe(int ax, int ay)
	{
		return !(abs(ax - x) > Width / 2 || abs(ay - y) > Height / 2);
	};
	void Fire()
	{
		Fire(theta);
	}
	void Fire(double theta)
	{
		double sx = 30 * cos(theta);
		double sy = 30 * sin(theta);
		Bullets.push_back(new Bullet(x, y, dx + sx, dy + sy));
	}
	void Shotgun()
	{
		Fire(theta);
		Fire(theta - 0.1);
		Fire(theta + 0.1);
	}
	bool Damage(HWND hWnd)
	{
		if (!Invincible)
		{
			MessageBeep(MB_OK);
			if (--Life >=0)
			{
				Invincible = true;
				++NumBomb;
				Bomb();
				SetTimer(hWnd, InvTimer, 2000, &Vulnerable);
				return true;
			}
			else
			{
				EndGame(hWnd, FALSE);
				return false;
			}
		}
		return true;
	}
	void SetTheta(int ax, int ay)
	{
		theta = atan2(double(ay-y), double(ax-x));
	}
	void Bomb()
	{
		if (NumBomb>0)
		{
			--NumBomb;
			for (double t = 0; t < PI*2; t += PI/8)
			{
				double sx = 3 * cos(t);
				double sy = 3 * sin(t);
				Bullets.push_back(new Barrier(x, y, sx, sy, 50));
			}
		}

	}
	void Freeze(HWND hWnd)
	{
		Frozen = true;
		SetTimer(hWnd, ThawTimer, 1000, &Thaw);
	}
} *Me;
HBITMAP Self::myImage;
HBITMAP Self::myInvincibleImage;
HBITMAP Self::myMaskImage;
HIMAGELIST Self::myAnimatedImage;
HIMAGELIST Self::myAnimatedInvincibleImage;
int Self::Width;
int Self::Height;

class Initializer
{
public:
	Initializer(HWND hWnd)
	{
		Entity::RandInt = std::bind(std::uniform_int_distribution < int > {INT_MIN, INT_MAX}, std::default_random_engine{ (unsigned int)time(0) });
		Entity::RandDbl = std::bind(std::uniform_real_distribution < double > {-MAXACCEL, MAXACCEL}, std::default_random_engine{ (unsigned int)time(0) });
		g_hInst = GetModuleHandle(NULL);
		Self::myImage = LoadBitmap(g_hInst, MAKEINTRESOURCE(IDB_BITMAP1));
		Self::myInvincibleImage = LoadBitmap(g_hInst, MAKEINTRESOURCE(IDB_BITMAP2));
		Self::myMaskImage = LoadBitmap(g_hInst, MAKEINTRESOURCE(IDB_BITMAP3));
		Self::myAnimatedImage = ImageList_LoadBitmap(g_hInst, MAKEINTRESOURCE(IDB_BITMAP5), 48, 8, RGB(255,0,255));
		Self::myAnimatedInvincibleImage = ImageList_LoadBitmap(g_hInst, MAKEINTRESOURCE(IDB_BITMAP7), 48, 8, RGB(255, 0, 255));
		BkBrush = CreatePatternBrush(LoadBitmap(g_hInst, MAKEINTRESOURCE(IDB_BITMAP8)));
		ImageList_GetIconSize(Self::myAnimatedImage, &Self::Height, &Self::Width);
		RECT R;
		GetClientRect(hWnd, &R);
		Entity::xLim = R.right;

		RECT Rct;
		SystemParametersInfo(SPI_GETWORKAREA, NULL, &Rct, NULL);

		Entity::yLim = R.bottom - (GetSystemMetrics(SM_CYFULLSCREEN) - (Rct.bottom - Rct.top));

		HDC hdc = GetDC(hWnd);
		SelectObject(hdc, GetStockObject(NULL_PEN));
		TargetPointer = CreatePen(PS_DASHDOT, 1, RGB(120,120,120));
		MemBit = CreateCompatibleBitmap(hdc, GetDeviceCaps(hdc, HORZRES), GetDeviceCaps(hdc, VERTRES));
		ReleaseDC(hWnd, hdc);
		NewGame(hWnd);
	}
	~Initializer()
	{
		delete Me;
		for (auto b : Balls)
			delete b;
		Balls.clear();
		DeleteObject(MemBit);
		DeleteObject(Self::myImage);
		DeleteObject(Self::myInvincibleImage);
		DeleteObject(Self::myMaskImage);
		DeleteObject(TargetPointer);
		DeleteObject(BkBrush);
		ImageList_Destroy(Self::myAnimatedImage);
	}
};

ATOM RegisterCustomClass(HINSTANCE& hInstance)
{
	WNDCLASS wc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.hCursor = LoadCursor(GetModuleHandle(NULL), MAKEINTRESOURCE(IDC_CURSOR1));
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hInstance = hInstance;
	wc.lpfnWndProc = WndProc;
	wc.lpszClassName = Title;
	wc.lpszMenuName = NULL;
	wc.style = CS_VREDRAW | CS_HREDRAW;
	return RegisterClass(&wc);
}
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	g_hInst = hInstance;
	RegisterCustomClass(hInstance);
	hMainWnd = CreateWindow(Title, Title, WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL);
	ShowWindow(hMainWnd, nCmdShow);
	MSG msg;
	while (GetMessage(&msg, 0, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	static Initializer* Init;
	switch (iMsg)
	{
	case WM_CREATE:
		Init = new Initializer(hWnd);
		InitCommonControls();
		hState = CreateStatusWindow(WS_CHILD | WS_VISIBLE, L"", hWnd, ID_STATUS);
		RECT R;
		RECT R2;
		SendMessage(hState, SB_GETRECT, 3, (LPARAM)&R);
		SendMessage(hState, SB_GETRECT, 5, (LPARAM)&R2);
		hProg = CreateWindow(PROGRESS_CLASS, 0, WS_CHILD | WS_VISIBLE | PBS_SMOOTH, R.left, R.top, R2.right - R.left, R2.bottom - R.top, hWnd, 0, g_hInst, 0);
		SetParent(hProg, hState);
		break;
	case WM_TIMER:
	{
		HDC hdc = GetDC(hWnd);
		HDC MemDC = CreateCompatibleDC(hdc);
		HBITMAP OldBit = (HBITMAP)SelectObject(MemDC, MemBit);
		RECT R;
		SetRect(&R, 0, 0, Entity::xLim, Entity::yLim);
		//FillRect(MemDC, &R, (HBRUSH)(Frozen ? GetStockObject(GRAY_BRUSH) : BkBrush));
		FillRect(MemDC, &R, (HBRUSH)(GetStockObject(Frozen ? GRAY_BRUSH : WHITE_BRUSH)));
		Me->Move();
		Me->Print(MemDC);

		for (auto &b : Balls)
		{
			if(!Frozen)
				b->Move();
			b->Push(0, 0.15);
			b->Print(MemDC);
			if (b->isEntityOnMe(Me))
			{

				if (!Me->Damage(hWnd)) break;
			}
		}


		for (auto i = Bullets.begin(); i != Bullets.end();)
		{
			Bullet* b = *i++;
			b->Move();
			b->Print(MemDC);
			if (b->InBound())
			{
				for (auto i = Balls.begin(); i != Balls.end();)
				{
					Ball* tmpBall = *i++;
					if (tmpBall->isEntityOnMe(b))
						tmpBall->split();
				}
			}
			else
			{
				Bullets.remove(b);
				delete b;
			}
		}
		SendMessage(hProg, PBM_SETPOS, CurPos, 0);
		SendMessage(hProg, PBM_SETRANGE, 0, MAKELPARAM(0, DestPos));
		if (CurPos == DestPos)
		{
			EndGame(hWnd, TRUE);
		}
		InvalidateRect(hWnd, NULL, FALSE);
		SelectObject(MemDC, OldBit);
		DeleteObject(MemDC);
		ReleaseDC(hWnd, hdc);
		break;
	}
	case WM_LBUTTONDOWN:
	{
		/*int x = LOWORD(lParam);
		int y = HIWORD(lParam);
		for (auto i = Balls.begin(); i != Balls.end();)
		{
			Ball* tmpBall = *i++;
			if (tmpBall->isPtOnMe(x,y))
				tmpBall->split();
		}*/
		Me->Fire();
		break;
	}
	case WM_RBUTTONDOWN:
	{
		Me->Shotgun();
		break;
	}
	case WM_KEYDOWN:
	{
		switch (wParam)
		{
		case VK_LEFT:
		case 'A':
			Me->Left = true;
			break;

		case 'D':
		case VK_RIGHT:
			Me->Right = true;
			break;

		case 'W':
		case VK_UP:
			Me->Up = true;
			break;

		case 'S':
		case VK_DOWN:
			Me->Down = true;
			break;
		case VK_SPACE:
		{
			Me->Bomb();
			break;
		}
		case VK_RETURN:
		{
			Me->Freeze(hWnd);
			break;
		}
		}
		break;

	}
	case WM_KEYUP:
	{
		switch (wParam)
		{
		case VK_LEFT:
		case 'A':
			Me->Left = false;
			break;
		case VK_RIGHT:
		case 'D':
			Me->Right = false;
			break;
		case 'W':
		case VK_UP:
			Me->Up = false;
			break;
		case 'S':
		case VK_DOWN:
			Me->Down = false;
			break;
		}
		break;
	}
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		HDC MemDC = CreateCompatibleDC(hdc);
		HBITMAP OldBit = (HBITMAP)SelectObject(MemDC, MemBit);
		BitBlt(hdc, 0, 0, Entity::xLim, Entity::yLim, MemDC, 0, 0, SRCCOPY);
		//DRAW
		EndPaint(hWnd, &ps);
		SelectObject(MemDC, OldBit);
		DeleteObject(MemDC);
		break;
	}
	case WM_SIZE:
	{
		RECT R;
		RECT Rct;
		GetClientRect(hWnd, &R);
		Entity::xLim = R.right;
		
		SystemParametersInfo(SPI_GETWORKAREA, NULL, &Rct, NULL);
		int SS = GetSystemMetrics(SM_CYFULLSCREEN);
		Entity::yLim = R.bottom + (GetSystemMetrics(SM_CYFULLSCREEN) - (Rct.bottom - Rct.top));

		SendMessage(hState, WM_SIZE, wParam, lParam);
		int SBPart[6];
		for (int i = 0; i < 6; ++i)
			SBPart[i] = LOWORD(lParam) / 6 * (i + 1);
		SendMessage(hState, SB_SETPARTS, 6, (LPARAM)SBPart);
		
		SendMessage(hState, SB_GETRECT, 3, (LPARAM)&R);
		SendMessage(hState, SB_GETRECT, 5, (LPARAM)&R2);
		SetWindowPos(hProg, 0, R.left, R.top, R2.right - R.left, R2.bottom - R.top, SWP_SHOWWINDOW);
		break;
	}
	case WM_MOUSEMOVE:
	{
		Me->SetTheta(LOWORD(lParam), HIWORD(lParam));
		break;
	}
	case WM_DESTROY:
		delete Init;
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, iMsg, wParam, lParam);
	}
	return 0;
}

VOID CALLBACK Vulnerable(HWND hWnd, UINT iMsg, UINT_PTR idEvent, DWORD dwTime)
{
	Me->Invincible = false;
	KillTimer(hWnd, idEvent);
}
VOID CALLBACK Thaw(HWND hWnd, UINT iMsg, UINT_PTR idEvent, DWORD dwTime)
{
	Frozen = false;
	KillTimer(hWnd, idEvent);
}
void EndGame(HWND hWnd,BOOL Clear)
{
	KillTimer(hWnd, FrameTimer);
	delete Me;
	Me = nullptr;
	for (auto &b : Balls)
		delete b;
	Balls.clear();

	if (MessageBox(hWnd, L"RESTART?", Clear?L"CLEAR":L"GAME OVER", MB_OKCANCEL) == IDOK)
		NewGame(hWnd);//initialize
	else
		PostQuitMessage(0);
}

void NewGame(HWND hWnd)
{
	DestPos = CurPos = 0;
	Balls.resize(BALLMAX);
	std::generate(Balls.begin(), Balls.end(), [](){return new Ball(); });
	Me = new Self();

	SetTimer(hWnd,InvTimer, 3000, &Vulnerable);
	SetTimer(hWnd,FrameTimer, FrameRate, NULL);
	SetTimer(hWnd, DiffTimer, 10000, &DifficultyRise);
}

VOID CALLBACK DifficultyRise(HWND hWnd, UINT iMsg, UINT_PTR CallerID, DWORD dwTime)
{
	for (auto& b : Balls)
	{
		b->ax = 2 * (Me->x - b->x) / sqrt((Me->x - b->x)*(Me->x - b->x) + (Me->y - b->y)*(Me->y - b->y));
		b->ay = 2 * (Me->y - b->y) / sqrt((Me->x - b->x)*(Me->x - b->x) + (Me->y - b->y)*(Me->y - b->y));
	}
}

VOID CALLBACK LineDrawProc(int x, int y, LPARAM lpData)
{
	std::pair<HDC, double*>* Info = (std::pair<HDC, double*>*)lpData;
	HDC hdc = Info->first;
	double size = *(Info->second);
	if (size>0)
	{
		HPEN RandomPen = CreatePen(PS_SOLID, 2, RGB(Entity::RandInt() % 255, Entity::RandInt() % 255, Entity::RandInt() % 255));
		SelectObject(hdc, GetStockObject(NULL_BRUSH));
		HPEN TempPen = (HPEN)SelectObject(hdc, RandomPen);
		Ellipse(hdc, x - size, y - size, x + size, y + size);
		*Info->second -= 0.1;
		SelectObject(hdc, TempPen);
		DeleteObject(RandomPen);
	}

}