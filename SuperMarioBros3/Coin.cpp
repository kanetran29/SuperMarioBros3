#include "Coin.h"
#include "Utils.h"
#include "PlayScene.h"
#include "BreakableBrick.h"

CCoin::CCoin(int tag) : CGameObject() {
	CGameObject::SetTag(tag);
	if (tag == COIN_TYPE_INBRICK)
		isAppear = false;
	else
		isAppear = true;
	if (tag == COIN_TYPE_TRANSFORM)
		StartExist();
	state = COIN_STATE_IDLE;
	type = IGNORE;
}
void CCoin::Update(DWORD dt, vector<LPGAMEOBJECT>* coObjects)
{
	if (isDestroyed)
		return;
	CGameObject::Update(dt);
	y += dy;
	CMario* coinmario = {};
	CPlayScene* coinscene = (CPlayScene*)CGame::GetInstance()->GetCurrentScene();
	if (coinscene != NULL)
		coinmario = ((CPlayScene*)coinscene)->GetPlayer();
	if (state == COIN_STATE_IDLE)
	{
		float mLeft, mTop, mRight, mBottom;
		if (coinmario != NULL)
		{
			coinmario->GetBoundingBox(mLeft, mTop, mRight, mBottom);
			if (isColliding(mLeft, mTop, mRight, mBottom))
			{
				isAppear = false;
				isDestroyed = true;
				coinmario->AddScore(x,y,100,false,false); 
				coinmario->AddMoney(1);
			}

		}
		if (tag == COIN_TYPE_TRANSFORM)
		{
			//transfer to brick
			if (GetTickCount64() - exist_start >= COIN_EXIST_TIME)
			{
				CBreakableBrick* item = new CBreakableBrick();
				CAnimationSets* animation_sets = CAnimationSets::GetInstance();
				LPANIMATION_SET tmp_ani_set = animation_sets->Get(BREAKABLEBRICK_ANI_SET_ID);

				item->SetAnimationSet(tmp_ani_set);
				item->SetPosition(x, y);
				coinscene->PushBack(item);
				isDestroyed = true;
			}
		}
	}
	if (state == COIN_STATE_UP)
	{
		if (GetTickCount64() - timing_start >= COIN_FALLING_TIME)
		{
			SetState(COIN_STATE_DOWN);
			StartTiming();
		}
	}
	if (state == COIN_STATE_DOWN)
	{
		if (GetTickCount64() - timing_start >= COIN_FALLING_TIME)
		{
			isAppear = false;
			SetState(COIN_STATE_IDLE);
			isDestroyed = true;
			coinmario->AddScore(x,y);
			coinmario->AddMoney();
		}
	}
}

void CCoin::Render()
{
	if (!isAppear || isDestroyed)
		return;
	animation_set->at(0)->Render(x, y);
	RenderBoundingBox(0);
}

void CCoin::GetBoundingBox(float& l, float& t, float& r, float& b)
{
	l = x;
	t = y;
	r = x + COIN_BBOX_WIDTH;
	b = y + COIN_BBOX_HEIGHT;
}

void CCoin::SetState(int state)
{
	CGameObject::SetState(state);
	switch (state)
	{
	case COIN_STATE_IDLE:
		vx = vy = 0;
		break;
	case COIN_STATE_UP:
		vy = -0.2f;
		StartTiming();
		break;
	case COIN_STATE_DOWN:
		vy = 0.2f;
		break;
	}
}