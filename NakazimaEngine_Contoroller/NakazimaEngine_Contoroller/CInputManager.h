#pragma once
#include <windows.h>
#include <Xinput.h>

//------------------------------------------------------------------------------
// CInputManager
// キーボードおよびゲームパッド入力を管理するシングルトンクラス
// シングルトン化しているので、インスタンスは GetInstance() から取得
//------------------------------------------------------------------------------
class CInputManager
{
public:
    // インスタンス取得（唯一のインスタンスを返す）
    static CInputManager& GetInstance();

    // 毎フレーム呼ぶ更新処理
    void Update();

    //--------------------------------------
    // キーボード入力判定
    //--------------------------------------
    bool IsKeyPress(int key) const;   // キーが押されているか
    bool IsKeyTrigger(int key) const; // キーが押された瞬間か
    bool IsKeyRelease(int key) const; // キーが離された瞬間か

    //--------------------------------------
    // ゲームパッド入力判定
    //--------------------------------------
    bool IsPadPress(WORD button) const; // ボタンが押されているか
    bool IsPadTrigger(WORD button) const; // ボタンが押された瞬間か
    bool IsPadRelease(WORD button) const; // ボタンが離された瞬間か

    // アナログスティック（-1.0f ~ 1.0f に正規化済み）
    float GetThumbLX() const;
    float GetThumbLY() const;

    // トリガー入力（0~255）
    BYTE GetLeftTrigger() const;
    BYTE GetRightTrigger() const;

    // ゲームパッド振動設定
    void SetVibration(WORD leftMotor, WORD rightMotor);

private:
    // コンストラクタは private にしてシングルトン化
    CInputManager();

    // デストラクタ
    ~CInputManager() = default;

    // コピー・代入禁止
    CInputManager(const CInputManager&) = delete;
    CInputManager& operator=(const CInputManager&) = delete;

    //--------------------------------------
    // メンバ変数
    //--------------------------------------
    BYTE m_keyTable[256];     // 現在のキー状態
    BYTE m_oldKeyTable[256];  // 前フレームのキー状態

    XINPUT_STATE m_state;     // 現在のゲームパッド状態
    XINPUT_STATE m_oldstate;    //前フレームのゲームパッド状態
    XINPUT_VIBRATION m_vibration; // 振動設定
};
