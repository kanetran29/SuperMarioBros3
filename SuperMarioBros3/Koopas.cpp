#include "Koopas.h"
#include "Utils.h"
#include "Block.h"
#include "QuestionBrick.h"
#include "BreakableBrick.h"
#include "IntroScene.h"
#include "IntroObject.h"
CKoopas::CKoopas()
{
	nx = -1;
	SetState(KOOPAS_STATE_WALKING);
	//SetState(KOOPAS_STATE_IN_SHELL);
	//SetState(KOOPAS_STATE_SHELL_UP);
}

void CKoopas::GetBoundingBox(float& left, float& top, float& right, float& bottom)
{
	left = x;
	top = y;
	right = x + KOOPAS_BBOX_WIDTH;
	if (state == KOOPAS_STATE_IN_SHELL || state == KOOPAS_STATE_SPINNING|| state == KOOPAS_STATE_SHELL_UP)
	{
		bottom = y + KOOPAS_BBOX_SHELL_HEIGHT;
	}
	else
		bottom = y + KOOPAS_BBOX_HEIGHT;
}
bool  CKoopas::CalRevivable()
{
	CGame* game = CGame::GetInstance();
	float camX, camY;

	camX = game->GetCamX();
	camY = game->GetCamY();

	bool can_1 =(start_x > camX - GetWidth() - MARIO_BIG_BBOX_WIDTH && start_x < camX + SCREEN_WIDTH + MARIO_BIG_BBOX_WIDTH
		&& start_y > camY - (SCREEN_HEIGHT - game->GetScreenHeight()) && start_y < camY + SCREEN_HEIGHT);
	bool can_2 = (x >= camX - GetWidth() - MARIO_BIG_BBOX_WIDTH && x <= camX + SCREEN_WIDTH + MARIO_BIG_BBOX_WIDTH
		&& y >= camY - (SCREEN_HEIGHT - game->GetScreenHeight()) && y < camY + SCREEN_HEIGHT);
	if (can_1 == false && can_2 == false && state != KOOPAS_STATE_WALKING)
		return true;
	return false;
}
void CKoopas::Update(DWORD dt, vector<LPGAMEOBJECT>* coObjects)
{
	CGameObject::Update(dt, coObjects);
	CMario* mario = {};
	if (!dynamic_cast<CIntroScene*> (CGame::GetInstance()->GetCurrentScene()))
		mario = ((CPlayScene*)CGame::GetInstance()->GetCurrentScene())->GetPlayer();
	else
	{
		if (((CIntroScene*)CGame::GetInstance()->GetCurrentScene())->holder == NULL)
			mario = ((CIntroScene*)CGame::GetInstance()->GetCurrentScene())->GetPlayer();
		else
			mario = ((CIntroScene*)CGame::GetInstance()->GetCurrentScene())->holder;
	}
	if (!mario->isAtIntroScene)
	{
		if (GetTickCount64() - shell_start >= KOOPAS_SHELL_TIME && shell_start != 0 && state != KOOPAS_STATE_SPINNING)
		{
			shell_start = 0;
			StartReviving();
		}
		if (GetTickCount64() - reviving_start >= KOOPAS_REVIVE_TIME && reviving_start != 0 && state != KOOPAS_STATE_SPINNING)
		{
			reviving_start = 0;
			y -= (KOOPAS_BBOX_HEIGHT - KOOPAS_BBOX_SHELL_HEIGHT) + 1.0f;
			if (isBeingHeld)
			{
				isBeingHeld = false;
				mario->SetIsHolding(false);
			}
			SetState(KOOPAS_STATE_WALKING);
		}
		if (CalRevivable())
			Reset();
	}
	// Simple fall down
	if (!isBeingHeld && !mario->isAtIntroScene)
		vy += KOOPAS_GRAVITY * dt;

	vector<LPCOLLISIONEVENT> coEvents;
	vector<LPCOLLISIONEVENT> coEventsResult;

	coEvents.clear();

	CalcPotentialCollisions(coObjects, coEvents);
		
	float mLeft, mTop, mRight, mBottom;
	float oLeft, oTop, oRight, oBottom;
	if (mario != NULL)
	{
		GetBoundingBox(oLeft, oTop, oRight, oBottom);
		if (mario->isTurningTail)
		{
			mario->getTail()->GetBoundingBox(mLeft, mTop, mRight, mBottom);
			if (isColliding(floor(mLeft), mTop, ceil(mRight), mBottom))
			{
				SetState(KOOPAS_STATE_SHELL_UP);
				mario->getTail()->ShowHitEffect();
			}
		}
		if (!mario->isHolding && !mario->isTurningTail)
		{
			float mLeft, mTop, mRight, mBottom;
			mario->GetBoundingBox(mLeft, mTop, mRight, mBottom);
			isBeingHeld = false;
			if (isColliding(mLeft, mTop, mRight, mBottom))
			{
				if (state == KOOPAS_STATE_IN_SHELL||state == KOOPAS_STATE_SHELL_UP)
				{
					this->nx = mario->nx;
					this->SetState(KOOPAS_STATE_SPINNING);
					mario->StartKicking();
				}
				if (state == KOOPAS_STATE_WALKING && mTop <= oBottom  &&
					mario->untouchable == 0 && mario->isKicking == false && !mario->isTurningTail)
						mario->Attacked();
			}
		}
		if (isBeingHeld)
		{
			y = mario->y + KOOPAS_BBOX_SHELL_HEIGHT / 2;
			float tmp = mario->vx;
			if (tmp < 0)
				tmp = -1;
			if (tmp > 0)
				tmp = 1;
			if (tmp == 0)
				tmp = mario->nx;

			x = mario->x + tmp * (MARIO_BIG_BBOX_WIDTH);
			if (mario->level == MARIO_LEVEL_SMALL)
			{
				if (tmp > 0)
					x = mario->x + tmp * (MARIO_SMALL_BBOX_WIDTH);
				else
					x = mario->x + tmp * (KOOPAS_BBOX_WIDTH);
				y -= (MARIO_BIG_BBOX_HEIGHT - MARIO_SMALL_BBOX_HEIGHT);
			}
		}
	}
	// No collision occured, proceed normally
	if (coEvents.size() == 0||isBeingHeld)
	{
		x += dx;
		y += dy;
		if (!isBeingHeld && state == KOOPAS_STATE_WALKING && 
			(tag == KOOPAS_RED) && CanPullBack && (y - lastStanding_Y > 1.0f))
		{
			//DebugOut(L"[SHELL] x %f y %f newx %f newy %f\n", x, y,x - nx * KOOPAS_BBOX_WIDTH, lastStanding_Y);
			y = lastStanding_Y;
			x -= nx * KOOPAS_BBOX_WIDTH;
			nx = -nx;
			vx = -vx;			
		}
	}
	else
	{

		float min_tx, min_ty;
		int nx = 0, ny = 0;
		float rdx = 0;
		float rdy = 0;

		// TODO: This is a very ugly designed function!!!!
		FilterCollision(coEvents, coEventsResult, min_tx, min_ty, nx, ny, rdx, rdy);

		// block 
		float x0 = x, y0 = y;
		x = x0 + min_tx * dx + nx * PUSHBACK;
		y = y0 + min_ty * dy + ny * PUSHBACK;

		//if (nx != 0) vx = 0;

		if (state == KOOPAS_STATE_SHELL_UP)
			vx = 0;
		// Collision logic with others
		for (UINT i = 0; i < coEventsResult.size(); i++)
		{
			LPCOLLISIONEVENT e = coEventsResult[i];
			if (e->obj != NULL)
				if (e->obj->isDestroyed == true)
					continue;
			GetBoundingBox(mLeft, mTop, mRight, mBottom);
			if (dynamic_cast<CKoopas*>(e->obj)) // if e->obj is Koopas 
			{
				CKoopas* koopas = dynamic_cast<CKoopas*>(e->obj);
				this->vx = -this->vx;
				koopas->vx = -koopas->vx;
				this->nx = -this->nx;
				koopas->nx = -koopas->nx;
				if (e->nx != 0)
				{
					if (state == KOOPAS_STATE_SPINNING)
					{
						if (koopas->GetState() != KOOPAS_STATE_IN_SHELL)
							koopas->SetState(KOOPAS_STATE_SHELL_UP);
					}
				}
				else if (e->ny < 0)
						vy = -KOOPAS_JUMP_SPEED;
					else
					{
						y = y0;
						SetState(KOOPAS_STATE_IN_SHELL);
					}
			}
			if (dynamic_cast<CGoomba*>(e->obj))
			{
				CGoomba* goomba = dynamic_cast<CGoomba*>(e->obj);
				if (goomba->GetState() != GOOMBA_STATE_DIE && (this->GetState() == KOOPAS_STATE_SPINNING || isBeingHeld))
					goomba->SetState(GOOMBA_STATE_DIE_BY_TAIL);
				else
				{
					goomba->vx = -goomba->vx;
					goomba->nx = -goomba->nx;
					this->vx = -this->vx;
					this->nx = -this->nx;
				}
			}
			if (dynamic_cast<CBrick*>(e->obj))
			{
				if (mario->isAtIntroScene)
				{
					if(!((CIntroScene*)CGame::GetInstance()->GetCurrentScene())->isCustomSpeed)
						SetState(KOOPAS_STATE_IN_SHELL);
				}
				else
				{
					CBrick* object = dynamic_cast<CBrick*>(e->obj);
					//object->SetDebugAlpha(255);
					object->GetBoundingBox(oLeft, oTop, oRight, oBottom);
					if (e->ny != 0) vy = 0;
					if (e->ny < 0)
					{
						CanPullBack = true;
						lastStanding_Y = y;
						if (tag == KOOPAS_GREEN_PARA || tag == KOOPAS_RED_PARA)
							vy = -KOOPAS_JUMP_SPEED;
					}
					if (e->nx != 0)
					{
						if (ceil(mBottom) != oTop)
							vx = -vx;
					}
				}
			}
			if (dynamic_cast<CQuestionBrick*>(e->obj) && state == KOOPAS_STATE_SPINNING && e->nx != 0)
			{
				CQuestionBrick* tmp = dynamic_cast<CQuestionBrick*>(e->obj);
				if (tmp->state != QUESTIONBRICK_STATE_HIT)
					tmp->SetState(QUESTIONBRICK_STATE_HIT);
			}
			if (dynamic_cast<CBreakableBrick*>(e->obj) && state == KOOPAS_STATE_SPINNING && e->nx != 0)
			{
				CBreakableBrick* tmp = dynamic_cast<CBreakableBrick*>(e->obj);
				tmp->Break();
			}
			if (dynamic_cast<CBlock*>(e->obj))
			{
				if (e->ny < 0)
				{
					CanPullBack = true;
					lastStanding_Y = y;
					vy = 0;
				}	
				else
				{
					x = x0 + dx;
					if(state == KOOPAS_STATE_SHELL_UP)
						y = y0 + dy;
				}
				if (e->ny < 0 && (tag == KOOPAS_GREEN_PARA || tag == KOOPAS_RED_PARA))
				{
					y = e->obj->y - KOOPAS_BBOX_HEIGHT;
					vy = -KOOPAS_JUMP_SPEED;
				}
			}
			if (dynamic_cast<CFireBullet*>(e->obj))	SetState(KOOPAS_STATE_IN_SHELL);
		}
	}
	// clean up collision events
	for (UINT i = 0; i < coEvents.size(); i++) delete coEvents[i];
	//if (state == KOOPAS_STATE_WALKING)
	//	DebugOut(L"[KOOPAS] x: %f\t y: %f\t \n", x, y);
}

void CKoopas::Render()
{
	int ani = -1;
	if (state == KOOPAS_STATE_SHELL_UP) {
		ani = KOOPAS_ANI_SHELL_UP;
	}
	else if (state == KOOPAS_STATE_IN_SHELL)
	{
		ani = KOOPAS_ANI_SHELL;
	}
	else if (state == KOOPAS_STATE_SPINNING)
	{
		if (vx < 0)
			ani = KOOPAS_ANI_SPIN_LEFT;
		else
			ani = KOOPAS_ANI_SPIN_RIGHT;
	}
	else
	{
		if (vx < 0)
			ani = KOOPAS_ANI_WALKING_LEFT;
		else
			ani = KOOPAS_ANI_WALKING_RIGHT;
	}
	if(tag == KOOPAS_GREEN_PARA || tag == KOOPAS_RED_PARA)
		if (vx < 0)
			ani = KOOPAS_ANI_PARA_LEFT;
		else
			ani = KOOPAS_ANI_PARA_RIGHT;
	if (reviving_start != 0)
	{
		if(state == KOOPAS_STATE_IN_SHELL)
			ani = KOOPAS_ANI_SHAKE;
		if (state == KOOPAS_STATE_SHELL_UP)
			ani = KOOPAS_ANI_SHAKE_UP;
	}
	animation_set->at(ani)->Render(x, y);
	RenderBoundingBox();
}

void CKoopas::SetState(int state)
{
	CGameObject::SetState(state);
	switch (state)
	{
	case KOOPAS_STATE_SHELL_UP:
	{
		CMario* mario = {};
		if (!dynamic_cast<CIntroScene*> (CGame::GetInstance()->GetCurrentScene()))
			mario = ((CPlayScene*)CGame::GetInstance()->GetCurrentScene())->GetPlayer();
		else
		{
			if (((CIntroScene*)CGame::GetInstance()->GetCurrentScene())->holder == NULL)
				mario = ((CIntroScene*)CGame::GetInstance()->GetCurrentScene())->GetPlayer();
			else
				mario = ((CIntroScene*)CGame::GetInstance()->GetCurrentScene())->holder;
		}
		vy = -KOOPAS_DIE_DEFLECT_SPEED;
		if (x < mario->x)
			nx = -1;
		else
			nx = 1;
		vx = nx * KOOPAS_WALKING_SPEED;
		StartShell();
		break;
	}
	case KOOPAS_STATE_WALKING:
		if(tag == KOOPAS_GREEN_PARA)
			vx = nx * KOOPAS_PARA_WALKING_SPEED;
		else
			vx = nx*KOOPAS_WALKING_SPEED;
		break;
	case KOOPAS_STATE_SPINNING:
		if (nx > 0)
			vx = KOOPAS_WALKING_SPEED * 5;
		else
			vx = -KOOPAS_WALKING_SPEED * 5;
		break;
	case KOOPAS_STATE_IN_SHELL:
		vx = 0;
		StartShell();
		break;
	case KOOPAS_STATE_PARA:
		vy = -KOOPAS_JUMP_SPEED;
		vx = nx * KOOPAS_PARA_WALKING_SPEED;
		break;
	}

}
void CKoopas::Reset()
{
	x = start_x;
	y = start_y;
	SetState(KOOPAS_STATE_WALKING);
}