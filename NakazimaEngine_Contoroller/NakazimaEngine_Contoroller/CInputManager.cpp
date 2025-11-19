#include "CInputManager.h"
#include <algorithm>

//------------------------------------------------------------------------------
// インスタンス取得（唯一のインスタンスを返す）
//------------------------------------------------------------------------------
CInputManager& CInputManager::GetInstance()
{
    static CInputManager instance;
    return instance;
}

//------------------------------------------------------------------------------
// コンストラクタ
//------------------------------------------------------------------------------
CInputManager::CInputManager()
{
    // キー入力配列を初期化
    ZeroMemory(m_keyTable, sizeof(m_keyTable));
    ZeroMemory(m_oldKeyTable, sizeof(m_oldKeyTable));

    // ゲームパッド入力状態を初期化
    ZeroMemory(&m_state, sizeof(m_state));
    ZeroMemory(&m_oldstate, sizeof(m_oldstate));

    // 振動を初期化（停止状態）
    ZeroMemory(&m_vibration, sizeof(m_vibration));
}

//------------------------------------------------------------------------------
// 毎フレーム呼ぶ更新処理
// キーボードとゲームパッドの状態を取得して保持
//------------------------------------------------------------------------------
void CInputManager::Update()
{
    //--- キーボード更新 ---
    for (int i = 0; i < 256; ++i)
    {
        // 前フレームの状態を保存
        m_oldKeyTable[i] = m_keyTable[i];

        // 現在のキー状態を取得（押されているなら1、押されていなければ0）
        m_keyTable[i] = (GetAsyncKeyState(i) & 0x8000) ? 1 : 0;
    }

    //このフレームで入力したキーを保存
    m_oldstate = m_state;

    //--- ゲームパッド更新 ---

    ZeroMemory(&m_state, sizeof(XINPUT_STATE));
    DWORD dwResult = XInputGetState(0, &m_state); // プレイヤー1のみ入力を取る
    if (dwResult != ERROR_SUCCESS)
    {
        // 未接続の場合はすべて0
        ZeroMemory(&m_state.Gamepad, sizeof(XINPUT_GAMEPAD));
    }

    // アナログスティックのデッドゾーン処理
    if ((m_state.Gamepad.sThumbLX < XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE &&
        m_state.Gamepad.sThumbLX > -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) &&
        (m_state.Gamepad.sThumbLY < XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE &&
            m_state.Gamepad.sThumbLY > -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE))
    {
        // 微小な入力を0として無視
        m_state.Gamepad.sThumbLX = 0;
        m_state.Gamepad.sThumbLY = 0;
    }

    //--- 振動リセット ---
    ZeroMemory(&m_vibration, sizeof(XINPUT_VIBRATION));


}

//------------------------------------------------------------------------------
// キーボード判定関数
//------------------------------------------------------------------------------
bool CInputManager::IsKeyPress(int key) const
{
    return m_keyTable[key]; // 押されているか
}

bool CInputManager::IsKeyTrigger(int key) const
{
    return m_keyTable[key] && !m_oldKeyTable[key]; // 押された瞬間
}

bool CInputManager::IsKeyRelease(int key) const
{
    return !m_keyTable[key] && m_oldKeyTable[key]; // 離された瞬間
}

//------------------------------------------------------------------------------
// ゲームパッド判定
//------------------------------------------------------------------------------
bool CInputManager::IsPadPress(WORD button) const
{
    return (m_state.Gamepad.wButtons & button) != 0;
}

bool CInputManager::IsPadTrigger(WORD button) const
{
    return (m_state.Gamepad.wButtons & button) && !(m_oldstate.Gamepad.wButtons & button);
}

bool CInputManager::IsPadRelease(WORD button) const
{
    return !(m_state.Gamepad.wButtons & button) && (m_oldstate.Gamepad.wButtons & button);
}

// アナログスティック (-1.0f ~ 1.0f)
float CInputManager::GetThumbLX() const
{
    return m_state.Gamepad.sThumbLX / 32767.0f;
}

float CInputManager::GetThumbLY() const
{
    return m_state.Gamepad.sThumbLY / 32767.0f;
}

// トリガー入力 (0~255)
BYTE CInputManager::GetLeftTrigger() const
{
    return m_state.Gamepad.bLeftTrigger;
}

BYTE CInputManager::GetRightTrigger() const
{
    return m_state.Gamepad.bRightTrigger;
}

//------------------------------------------------------------------------------
// ゲームパッド振動設定
// leftMotor, rightMotor = 0~65535
//------------------------------------------------------------------------------
void CInputManager::SetVibration(WORD leftMotor, WORD rightMotor)
{
    m_vibration.wLeftMotorSpeed = leftMotor;
    m_vibration.wRightMotorSpeed = rightMotor;
    XInputSetState(0, &m_vibration);
}
