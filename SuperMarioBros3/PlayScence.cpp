#include <iostream>
#include <fstream>

#include "PlayScence.h"
#include "Utils.h"
#include "Textures.h"
#include "Sprites.h"
#include "Portal.h"
#include "Map.h"
#include "Block.h"
#include "Define.h"

using namespace std;

CPlayScene::CPlayScene(int id, LPCWSTR filePath) :
	CScene(id, filePath)
{
	key_handler = new CPlayScenceKeyHandler(this);
}

/*
	Load scene resources from scene file (textures, sprites, animations and objects)
	See scene1.txt, scene2.txt for detail format specification
*/

#define SCENE_SECTION_UNKNOWN -1
#define SCENE_SECTION_TEXTURES 2
#define SCENE_SECTION_SPRITES 3
#define SCENE_SECTION_ANIMATIONS 4
#define SCENE_SECTION_ANIMATION_SETS	5
#define SCENE_SECTION_OBJECTS	6
#define SCENE_SECTION_TILEMAP_DATA	7

#define OBJECT_TYPE_MARIO	0
#define OBJECT_TYPE_BRICK	1
#define OBJECT_TYPE_GOOMBA	2
#define OBJECT_TYPE_KOOPAS	3
#define OBJECT_TYPE_BLOCK	4
#define OBJECT_TYPE_FIRE_BULLET	9

#define OBJECT_TYPE_PORTAL	50

#define MAX_SCENE_LINE 1024

Map* TileMap;
void CPlayScene::_ParseSection_TEXTURES(string line)
{
	vector<string> tokens = split(line);

	if (tokens.size() < 5) return; // skip invalid lines

	int texID = atoi(tokens[0].c_str());
	wstring path = ToWSTR(tokens[1]);
	int R = atoi(tokens[2].c_str());
	int G = atoi(tokens[3].c_str());
	int B = atoi(tokens[4].c_str());

	CTextures::GetInstance()->Add(texID, path.c_str(), D3DCOLOR_XRGB(R, G, B));
}

void CPlayScene::_ParseSection_SPRITES(string line)
{
	vector<string> tokens = split(line);

	if (tokens.size() < 6) return; // skip invalid lines

	int ID = atoi(tokens[0].c_str());
	int l = atoi(tokens[1].c_str());
	int t = atoi(tokens[2].c_str());
	int r = atoi(tokens[3].c_str());
	int b = atoi(tokens[4].c_str());
	int texID = atoi(tokens[5].c_str());

	LPDIRECT3DTEXTURE9 tex = CTextures::GetInstance()->Get(texID);
	if (tex == NULL)
	{
		DebugOut(L"[ERROR] Texture ID %d not found!\n", texID);
		return;
	}

	CSprites::GetInstance()->Add(ID, l, t, r, b, tex);
}

void CPlayScene::_ParseSection_ANIMATIONS(string line)
{
	vector<string> tokens = split(line);

	if (tokens.size() < 3) return; // skip invalid lines - an animation must at least has 1 frame and 1 frame time

	//DebugOut(L"--> %s\n",ToWSTR(line).c_str());

	LPANIMATION ani = new CAnimation();

	int ani_id = atoi(tokens[0].c_str());
	for (int i = 1; i < tokens.size(); i += 2)	// why i+=2 ?  sprite_id | frame_time  
	{
		int sprite_id = atoi(tokens[i].c_str());
		int frame_time = atoi(tokens[i + 1].c_str());
		ani->Add(sprite_id, frame_time);
	}

	CAnimations::GetInstance()->Add(ani_id, ani);
}

void CPlayScene::_ParseSection_ANIMATION_SETS(string line)
{
	vector<string> tokens = split(line);

	if (tokens.size() < 2) return; // skip invalid lines - an animation set must at least id and one animation id

	int ani_set_id = atoi(tokens[0].c_str());
	LPANIMATION_SET s;
	if (CAnimationSets::GetInstance()->animation_sets[ani_set_id] != NULL)
		s = CAnimationSets::GetInstance()->animation_sets[ani_set_id];
	else
		s = new CAnimationSet();
	CAnimations* animations = CAnimations::GetInstance();

	for (int i = 1; i < tokens.size(); i++)
	{
		int ani_id = atoi(tokens[i].c_str());

		LPANIMATION ani = animations->Get(ani_id);
		s->push_back(ani);
	}
	CAnimationSets::GetInstance()->Add(ani_set_id, s);
}

/*
	Parse a line in section [OBJECTS]
*/
void CPlayScene::_ParseSection_OBJECTS(string line)
{
	vector<string> tokens = split(line);

	//DebugOut(L"--> %s\n",ToWSTR(line).c_str());

	if (tokens.size() < 3) return; // skip invalid lines - an object set must have at least id, x, y
	int tag = 0;
	int object_type = atoi(tokens[0].c_str());
	float x = atof(tokens[1].c_str());
	float y = atof(tokens[2].c_str());

	int ani_set_id = atoi(tokens[3].c_str());
	if (tokens.size() == 5)
		tag = atof(tokens[4].c_str());

	CAnimationSets* animation_sets = CAnimationSets::GetInstance();

	CGameObject* obj = NULL;
	switch (object_type)
	{
	case OBJECT_TYPE_MARIO:
		if (player != NULL)
		{
			DebugOut(L"[ERROR] MARIO object was created before!\n");
			return;
		}
		obj = new CMario(x, y);
		player = (CMario*)obj;
		DebugOut(L"[INFO] Player object created!\n");
		break;
	case OBJECT_TYPE_GOOMBA:
		obj = new CGoomba();
		obj->SetTag(tag);
		break;
	case OBJECT_TYPE_BRICK: 
		obj = new CBrick();
		obj->SetTag(tag);
		break;
	case OBJECT_TYPE_KOOPAS:
		obj = new CKoopas();
		obj->SetTag(tag);
		break;
	case OBJECT_TYPE_BLOCK:
		obj = new CBlock();
		obj->SetTag(tag);
		break;
	case OBJECT_TYPE_FIRE_BULLET: obj = new CFireBullet(); break;
		//case OBJECT_TYPE_PORTAL:
		//	{	
		//		float r = atof(tokens[4].c_str());
		//		float b = atof(tokens[5].c_str());
		//		int scene_id = atoi(tokens[6].c_str());
		//		obj = new CPortal(x, y, r, b, scene_id);
		//	}
		//	break;
	default:
		DebugOut(L"[ERR] Invalid object type: %d\n", object_type);
		return;
	}

	// General object setup
	obj->SetPosition(x, y);

	LPANIMATION_SET ani_set = animation_sets->Get(ani_set_id);

	obj->SetAnimationSet(ani_set);
	if (object_type == OBJECT_TYPE_FIRE_BULLET && player != NULL)
	{
		CFireBullet* f = dynamic_cast<CFireBullet*>(obj);
		player->AddBullets(f);
	}
	objects.push_back(obj);
}
void CPlayScene::_ParseSection_TILEMAP_DATA(string line)
{
	int ID, rowMap, columnMap, columnTile, rowTile, totalTiles;
	LPCWSTR path = ToLPCWSTR(line);
	ifstream f;

	f.open(path);
	f >> ID >> rowMap >> columnMap >> rowTile >> columnTile >> totalTiles;
	//Init Map Matrix

	int** TileMapData = new int* [rowMap];
	for (int i = 0; i < rowMap; i++)
	{
		TileMapData[i] = new int[columnMap];
		for (int j = 0; j < columnMap; j++)
			f >> TileMapData[i][j];
	}
	f.close();

	TileMap = new Map(ID, rowMap, columnMap, rowTile, columnTile, totalTiles);
	TileMap->ExtractTileFromTileSet();
	TileMap->SetTileMapData(TileMapData);
	DebugOut(L"[DETAILS] rowmap: %d	%d	%d	%d	%d\n", rowMap, columnMap, columnTile, rowTile, totalTiles);
}
void CPlayScene::Load()
{
	DebugOut(L"[INFO] Start loading scene resources from : %s \n", sceneFilePath);

	ifstream f;
	f.open(sceneFilePath);

	// current resource section flag
	int section = SCENE_SECTION_UNKNOWN;

	char str[MAX_SCENE_LINE];
	while (f.getline(str, MAX_SCENE_LINE))
	{
		string line(str);

		if (line[0] == '#') continue;	// skip comment lines	

		if (line == "[TEXTURES]") { section = SCENE_SECTION_TEXTURES; continue; }
		if (line == "[SPRITES]") {
			section = SCENE_SECTION_SPRITES; continue;
		}
		if (line == "[TILEMAP DATA]") {
			section = SCENE_SECTION_TILEMAP_DATA; continue;
		}
		if (line == "[ANIMATIONS]") {
			section = SCENE_SECTION_ANIMATIONS; continue;
		}
		if (line == "[ANIMATION_SETS]") {
			section = SCENE_SECTION_ANIMATION_SETS; continue;
		}
		if (line == "[OBJECTS]") {
			section = SCENE_SECTION_OBJECTS; continue;
		}
		if (line[0] == '[') { section = SCENE_SECTION_UNKNOWN; continue; }
		//
		// data section
		//
		switch (section)
		{
		case SCENE_SECTION_TEXTURES: _ParseSection_TEXTURES(line); break;
		case SCENE_SECTION_SPRITES: _ParseSection_SPRITES(line); break;
		case SCENE_SECTION_ANIMATIONS: _ParseSection_ANIMATIONS(line); break;
		case SCENE_SECTION_ANIMATION_SETS: _ParseSection_ANIMATION_SETS(line); break;
		case SCENE_SECTION_OBJECTS: _ParseSection_OBJECTS(line); break;
		case SCENE_SECTION_TILEMAP_DATA: _ParseSection_TILEMAP_DATA(line); break;
		}
	}

	f.close();

	CTextures::GetInstance()->Add(ID_TEX_BBOX, L"Resources\\Textures\\bbox.png", D3DCOLOR_XRGB(255, 255, 255));

	DebugOut(L"[INFO] Done loading scene resources %s\n", sceneFilePath);
}

void CPlayScene::Update(DWORD dt)
{
	// We know that Mario is the first object in the list hence we won't add him into the colliable object list
	// TO-DO: This is a "dirty" way, need a more organized way 
	//for (auto i = objects.begin() + 1; i != objects.end(); ++i)
	//{
	//	if (dynamic_cast<CGoomba*>(*i))
	//	{
	//		CGoomba* goomba = dynamic_cast<CGoomba*>(*i);
	//		if (goomba->GetState() == GOOMBA_STATE_DIE)
	//		{
	//			objects.erase(i);
	//			i--;
	//			delete goomba;
	//		}
	//	}
	//}
	vector<LPGAMEOBJECT> coObjects;
	for (size_t i = 1; i < objects.size(); i++)
	{
		coObjects.push_back(objects[i]);
	}

	for (size_t i = 0; i < objects.size(); i++)
	{
		objects[i]->Update(dt, &coObjects);
	}

	// skip the rest if scene was already unloaded (Mario::Update might trigger PlayScene::Unload)
	if (player == NULL) return;


	//get map and screen information
	float cx, cy, sw, sh, mw, mh, mx, my;
	player->GetPosition(cx, cy);
	CGame* game = CGame::GetInstance();
	sw = game->GetScreenWidth();
	sh = game->GetScreenHeight();
	mw = TileMap->GetMapWidth();
	mh = TileMap->GetMapHeight();

	// Update camera to follow mario
	if (cx >= sw / 2 //Left Edge
		&& cx + sw / 2 <= mw) //Right Edge
		cx -= sw / 2;
	else if (cx < sw / 2)
		cx = 0;
	else if (cx + sw / 2 > mw)
		cx = mw - sw + 1;
	if (cy - sh / 2 <= 0)//Top Edge
		cy = 0;
	else if (cy + sh / 2 >= mh)//Bottom Edge
		cy = mh - sh;
	else cy -= sh / 2;
	CGame::GetInstance()->SetCamPos((int)cx, (int)cy);
	//TileMap->SetCamPos((int)cx, (int)cy);
	TileMap->SetCamPos(cx, cy);
}

void CPlayScene::Render()
{
	TileMap->Render();
	for (int i = 0; i < objects.size(); i++)
		objects[i]->Render();
}

/*
	Unload current scene
*/
void CPlayScene::Unload()
{
	for (int i = 0; i < objects.size(); i++)
		delete objects[i];

	objects.clear();
	player = NULL;

	DebugOut(L"[INFO] Scene %s unloaded! \n", sceneFilePath);
}
int cc = 0;
void CPlayScenceKeyHandler::OnKeyDown(int KeyCode)
{
	//DebugOut(L"[INFO] KeyDown: %d\n", KeyCode);

	CMario* mario = ((CPlayScene*)scence)->GetPlayer();
	switch (KeyCode)
	{
	case DIK_1:
		mario->SetLevel(MARIO_LEVEL_SMALL);
		break;
	case DIK_2:
		mario->SetLevel(MARIO_LEVEL_BIG);
		break;
	case DIK_3:
		mario->SetLevel(MARIO_LEVEL_TAIL);
		break;
	case DIK_4:
		mario->SetLevel(MARIO_LEVEL_FIRE);
		break;
	case DIK_B:
		if (mario->level == MARIO_LEVEL_FIRE && !mario->isShooting && !mario->isSitting)
		{
			mario->StartShooting(mario->x, mario->y);
			//DebugOut(L"%f %f\n", mario->x, mario->y);
		}
		if (mario->level == MARIO_LEVEL_TAIL && !mario->isTurningTail && !mario->isSitting)
			mario->StartTurning();
		break;
	case DIK_A:
		mario->Reset();
		break;
	case DIK_SPACE:
		if (mario->isOnGround)
		{
			mario->SetIsReadyToJump(true);
			//mario->
		}
		else if (mario->level == MARIO_LEVEL_TAIL && !mario->isFlapping && mario->vy >0)
		{
			mario->StartFlapping();
		}
		break;
	}
}
void CPlayScenceKeyHandler::OnKeyUp(int KeyCode)
{
	CMario* mario = ((CPlayScene*)scence)->GetPlayer();
	switch (KeyCode)
	{
	case DIK_SPACE:
		if (!mario->isOnGround)
		{
			mario->SetIsReadyToJump(false);
			mario->SetIsFlapping(false);
		}
		//DebugOut(L"[INFO] Is not on ground \n");
		break;
	case DIK_DOWN:
		mario->SetIsSitting(false);
	case DIK_B:
		mario->SetIsHolding(false);
		mario->SetIsReadyToHold(false);
		if (mario->isHolding)
		{
			mario->StartKicking();
		}
		//mario->SetIs
		break;
		/*case DIK_DOWN:
			mario->SetPosition(mario->x, mario->y - 9);*/
			//case DIK_RIGHT:
			//	mario->SetState(MARIO_STATE_INERTIA);
			//	break;
			//case DIK_LEFT:
			//	mario->SetState(MARIO_STATE_INERTIA);
			//	break;
	}
}
void CPlayScenceKeyHandler::KeyState(BYTE* states)
{
	CGame* game = CGame::GetInstance();
	CMario* mario = ((CPlayScene*)scence)->GetPlayer();

	// disable control key when Mario die 
	if (mario->GetState() == MARIO_STATE_DIE) return;
	if (game->IsKeyDown(DIK_B))
		mario->SetIsReadyToHold(true);
	if (game->IsKeyDown(DIK_SPACE) && mario->isReadyToJump)
	{
		mario->SetState(MARIO_STATE_JUMPING);
		mario->SetIsJumping(true);
		mario->SetIsReadyToSit(false);
	}
	else if (game->IsKeyDown(DIK_RIGHT))
		mario->SetState(MARIO_STATE_WALKING_RIGHT);
	else if (game->IsKeyDown(DIK_LEFT))
		mario->SetState(MARIO_STATE_WALKING_LEFT);
	else if (game->IsKeyDown(DIK_DOWN) && mario->isReadyToSit)
		mario->SetState(MARIO_STATE_SITTING);
	else mario->SetState(MARIO_STATE_IDLE);
}