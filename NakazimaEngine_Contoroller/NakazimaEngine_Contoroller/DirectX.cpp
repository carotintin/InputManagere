#include "Main.h"
#include "DirectX.h"
#include "CInputManager.h"


//--------------------------------------------------------------------------------------
// DirectX11::DirectX11()関数：コンストラクタ
//--------------------------------------------------------------------------------------
// ここは DirectX11 クラスのコンストラクタです。
// 現在は初期化処理をコンストラクタ内で行っておらず、InitDevice() で初期化します。
// コンストラクタ自体は空で問題ない設計（リソース確保は InitDevice() 側）です。
DirectX11::DirectX11()
{

}

//--------------------------------------------------------------------------------------
// DirectX11::~DirectX11関数：デストラクタ
//--------------------------------------------------------------------------------------
// デストラクタも空です。ComPtr を使っているため、オブジェクト破棄時に自動で
// COM リソースが解放されます。明示的な Release は不要です（設計上の簡潔化）。
DirectX11::~DirectX11()
{

}

//--------------------------------------------------------------------------------------
// DirectX11::InitDevice()：DirectX関係の初期化
//--------------------------------------------------------------------------------------
// InitDevice() は Direct3D / Direct2D / DirectWrite / DXGI (スワップチェーン)
// といった描画に必要なすべての初期化を行う関数です。
// 成功したら S_OK を返し、失敗したらエラーコードを返します。
// この中で「デバイスの作成」「Direct2D デバイス/コンテキスト作成」
// 「スワップチェーン作成」「バックバッファの Direct2D への紐付け」
// 「レンダーターゲットビューの作成」「フォント・ブラシの作成」などを行います。
HRESULT DirectX11::InitDevice()
{
    //------------------------------------------------------------
    // DirectX11とDirect2D 1.1の初期化
    //------------------------------------------------------------
    // HRESULT は Windows のエラーコード。S_OK（成功）か FAILED(hr) をチェックします。
    HRESULT hr = S_OK;

    // Direct2D と連携するために BGRA サポートを有効にするフラグ。
    // Direct2D は BGRA レイアウトのテクスチャを期待することが多いので必要です。
    UINT uiDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;//DirectX11上でDirect2Dを使用するために必要
#ifdef _DEBUG
    // デバッグビルドではデバッグ用フラグを追加して、デバッグ情報を得られるようにします。
    uiDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    // 利用可能なドライバタイプの一覧を用意。まずはハードウェアを試し、
    // ダメなら WARP（ソフトウェア）、最終的にはリファレンスを試すという形。
    D3D_DRIVER_TYPE drivertypes[] =
    {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    UINT uiDriverTypesNum = ARRAYSIZE(drivertypes);

    // サポートする機能レベルの候補を上から並べる
    // (例: 11.1 -> 11.0 -> 10.1 -> 10.0)
    D3D_FEATURE_LEVEL featurelevels[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    UINT uiFeatureLevelsNum = ARRAYSIZE(featurelevels);

    // ローカル変数としての D3D デバイス（ComPtr）。成功したらここに入る。
    Microsoft::WRL::ComPtr<ID3D11Device> D3DDevice;
    D3D_DRIVER_TYPE drivertype = D3D_DRIVER_TYPE_NULL;
    D3D_FEATURE_LEVEL featurelevel = D3D_FEATURE_LEVEL_11_0;

    // ドライバタイプを順番に試すループ（ハードウェア → WARP → リファレンス）
    for (UINT uiDriverTypeIndex = 0; uiDriverTypeIndex < uiDriverTypesNum; uiDriverTypeIndex++)
    {
        drivertype = drivertypes[uiDriverTypeIndex];
        // D3D11CreateDevice でデバイスとコンテキストを取得する。
        // 第1引数=nullptr は既定のアダプタ（通常は最初の GPU）を使う指定。
        // featurelevels 配列でサポートしたい機能レベルを伝え、取得できたものが featurelevel に入る。
        hr = D3D11CreateDevice(nullptr, drivertype, nullptr, uiDeviceFlags, featurelevels, uiFeatureLevelsNum,
            D3D11_SDK_VERSION, &D3DDevice, &featurelevel, &m_D3DDeviceContext);//&D3DDevice &m_D3DDeviceContext 初期化

        // 古い環境などで D3D_FEATURE_LEVEL_11_1 が不正引数扱いになることがあるため
        // その場合は featurelevels の先頭をずらして再試行する処理を行っている。
        if (hr == E_INVALIDARG)
        {
            hr = D3D11CreateDevice(nullptr, drivertype, nullptr, uiDeviceFlags, &featurelevels[1], uiFeatureLevelsNum - 1,
                D3D11_SDK_VERSION, &D3DDevice, &featurelevel, &m_D3DDeviceContext);//&D3DDevice &m_D3DDeviceContext 初期化
        }

        // 成功したらループを抜け、D3D デバイスが確保される
        if (SUCCEEDED(hr))
            break;
    }
    // すべてのドライバ種別で失敗した場合は初期化失敗を返す
    if (FAILED(hr))
        return hr;

    //--------------------------------------------------------------------------------
    // 次に Direct2D (D2D1) と連携するために、D3D デバイスを DXGI デバイスに変換する
    //--------------------------------------------------------------------------------
    // Microsoft::WRL::ComPtr::As() を使って QueryInterface 相当の変換を行う。
    Microsoft::WRL::ComPtr<IDXGIDevice2> DXGIDevice2;
    hr = D3DDevice.As(&DXGIDevice2);//D3DDevice->QueryInterface()ではなくD3DDevice.As()、&DXGIDevice2 初期化
    if (FAILED(hr))
        return hr;

    //--------------------------------------------------------------------------------
    // Direct2D ファクトリ（ID2D1Factory1）の作成
    //--------------------------------------------------------------------------------
    Microsoft::WRL::ComPtr<ID2D1Factory1> D2DFactory1;
    D2D1_FACTORY_OPTIONS factoryoptions = {};
#ifdef _DEBUG
    // デバッグ時は Direct2D のデバッグレベルを上げておく（診断情報が得やすくなる）
    factoryoptions.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
    // D2D1CreateFactory によって Direct2D のファクトリを生成する。
    // GetAddressOf() を使って ComPtr のアドレスを渡すのが WRL の慣習。
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factoryoptions, D2DFactory1.GetAddressOf());//&D2DFactory1ではなくD2DFactory1.GetAddressOf()
    if (FAILED(hr))
        return hr;

    //--------------------------------------------------------------------------------
    // DXGI デバイスを使って Direct2D のデバイスを作成し、さらに DeviceContext を取得する
    //--------------------------------------------------------------------------------
    Microsoft::WRL::ComPtr<ID2D1Device> D2D1Device;
    // DXGIDevice2.Get() を渡して Direct2D 側のデバイスを生成
    hr = D2DFactory1->CreateDevice(DXGIDevice2.Get(), &D2D1Device);//DXGIDevice2ではなくDXGIDevice2.Get()、&D2D1Device 初期化
    if (FAILED(hr))
        return hr;

    // デバイスから DeviceContext を取得する（2D の描画命令はこのコンテキスト経由）
    hr = D2D1Device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_D2DDeviceContext);//&m_D2DDeviceContext 初期化
    if (FAILED(hr))
        return hr;

    //--------------------------------------------------------------------------------
    // スワップチェーン（DXGI）を作るためにアダプタとファクトリを取得する
    //--------------------------------------------------------------------------------
    Microsoft::WRL::ComPtr<IDXGIAdapter> DXGIAdapter;
    hr = DXGIDevice2->GetAdapter(&DXGIAdapter);//&DXGIAdapter 初期化
    if (FAILED(hr))
        return hr;

    Microsoft::WRL::ComPtr<IDXGIFactory2> DXGIFactory2;
    // Adapter の親が Factory（IDXGIFactory2）なので GetParent で取得する
    hr = DXGIAdapter->GetParent(IID_PPV_ARGS(&DXGIFactory2));//&DXGIFactory2 初期化
    if (FAILED(hr))
        return hr;

    //--------------------------------------------------------------------------------
    // スワップチェーンの設定（DXGI_SWAP_CHAIN_DESC1）
    //--------------------------------------------------------------------------------
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    // ウィンドウのクライアント領域サイズ（Window クラスの static メソッドから取得）
    desc.Width = Window::GetClientWidth();
    desc.Height = Window::GetClientHeight();
    // バッファのフォーマット（8bit B,G,R,A）
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.Stereo = FALSE;                       // ステレオ（3D）出力は使わない
    desc.SampleDesc.Count = 1;                // マルチサンプルは使わない（1）
    desc.SampleDesc.Quality = 0;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // レンダーターゲットとして使用
    desc.BufferCount = 2;                      // バッファ数（2 = ダブルバッファ）
    desc.Scaling = DXGI_SCALING_STRETCH;       // スケーリング方法
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD; // スワップの方式（古いタイプだが互換性あり）
    desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

    // スワップチェーンの生成。D3DDevice.Get()（生ポインタ）とウィンドウハンドルを渡す。
    hr = DXGIFactory2->CreateSwapChainForHwnd(D3DDevice.Get(), Window::GethWnd(), &desc, nullptr, nullptr, &m_DXGISwapChain1);//D3DDeviceではなくD3DDevice.Get()、&m_DXGISwapChain1 初期化
    if (FAILED(hr))
        return hr;

    // フレームレイテンシ（最大遅延フレーム数）を 1 に設定（遅延を減らす）
    (void)DXGIDevice2->SetMaximumFrameLatency(1);

    // Alt+Enter によるフルスクリーン切り替えを無効にする（アプリ側で制御したい場合）
    DXGIFactory2->MakeWindowAssociation(Window::GethWnd(), DXGI_MWA_NO_ALT_ENTER);//Alt+Enter時フルスクリーンを無効

    //--------------------------------------------------------------------------------
    // スワップチェーンのバックバッファ（IDXGISurface2）を取得して、Direct2D 用の
    // Bitmap に変換する。これにより Direct2D の描画結果がスワップチェーンに反映される。
    //--------------------------------------------------------------------------------
    Microsoft::WRL::ComPtr<IDXGISurface2> DXGISurface2;
    hr = m_DXGISwapChain1->GetBuffer(0, IID_PPV_ARGS(&DXGISurface2));//&DXGISurface2 初期化
    if (FAILED(hr))
        return hr;

    // Direct2D のビットマップとしてラップする際のプロパティ指定。
    // D2D1_BITMAP_OPTIONS_TARGET を指定することで、Direct2D の描画ターゲットとして使用可能にする。
    hr = m_D2DDeviceContext->CreateBitmapFromDxgiSurface(DXGISurface2.Get(),//DXGISurface2ではなくDXGISurface2.Get()
        D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE)), &m_D2DBitmap1);//&m_D2DBitmap1 初期化
    if (FAILED(hr))
        return hr;

    // DeviceContext の描画ターゲットにセット（以降の Direct2D 描画はこのターゲットに描かれる）
    m_D2DDeviceContext->SetTarget(m_D2DBitmap1.Get());//D2DBitmap1ではなくD2DBitmap1.Get()

    //--------------------------------------------------------------------------------
    // Direct3D 側でもレンダーターゲットビューを作成しておく（3D 描画を混在させるため）
    //--------------------------------------------------------------------------------
    Microsoft::WRL::ComPtr<ID3D11Texture2D> D3DTexture2D;
    hr = m_DXGISwapChain1->GetBuffer(0, IID_PPV_ARGS(&D3DTexture2D));//&D3DTexture2D 初期化
    if (FAILED(hr))
        return hr;

    // バックバッファ（テクスチャ）からレンダーターゲットビューを作っておく
    hr = D3DDevice->CreateRenderTargetView(D3DTexture2D.Get(), nullptr, &m_D3DRenderTargetView);//D3DTexture2DではなくD3DTexture2D.Get()、&m_D3DRenderTargetView 初期化
    if (FAILED(hr))
        return hr;

    // 作成したレンダーターゲットビューを OM (Output Merger) ステージにセットする
    m_D3DDeviceContext->OMSetRenderTargets(1, m_D3DRenderTargetView.GetAddressOf(), nullptr);//&m_D3DRenderTargetViewではなくm_D3DRenderTargetView.GetAddressOf()

    // ビューポート（描画領域）を設定する
    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)Window::GetClientWidth();
    vp.Height = (FLOAT)Window::GetClientHeight();
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    m_D3DDeviceContext->RSSetViewports(1, &vp);

    //------------------------------------------------------------
    // DirectWriteの初期化
    //------------------------------------------------------------
    // DirectWrite を使ってテキスト描画用のフォントと整列方法を設定する
    Microsoft::WRL::ComPtr<IDWriteFactory> DWriteFactory;
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), &DWriteFactory);//&DWriteFactory 初期化
    if (FAILED(hr))
        return hr;

    // 関数 CreateTextFormat() のパラメータ説明（コメント）
    // 第1引数：フォント名（L"メイリオ", L"Arial", L"Meiryo UI"等）
    // 第2引数：フォントコレクション（nullptr）
    // 第3引数：フォントの太さ（DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_WEIGHT_BOLD等）
    // 第4引数：フォントスタイル（DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STYLE_OBLIQUE, DWRITE_FONT_STYLE_ITALIC）
    // 第5引数：フォントの幅（DWRITE_FONT_STRETCH_NORMAL,DWRITE_FONT_STRETCH_EXTRA_EXPANDED等）
    // 第6引数：フォントサイズ（20, 30等）
    // 第7引数：ロケール名（L""）
    // 第8引数：テキストフォーマット（&g_pTextFormat）
    hr = DWriteFactory->CreateTextFormat(L"メイリオ", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 20, L"", &m_DWriteTextFormat);//&m_DWriteTextFormat 初期化
    if (FAILED(hr))
        return hr;

    // テキストの横方向の整列（左揃え）を指定
    hr = m_DWriteTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    if (FAILED(hr))
        return hr;

    // CreateSolidColorBrush() で使用するブラシ（文字色）を作る
    // D2D の ColorF で色指定（ここでは黒）
    hr = m_D2DDeviceContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &m_D2DSolidBrush);//&m_D2DSolidBrush 初期化
    if (FAILED(hr))
        return hr;

    // すべて成功したら S_OK を返す
    return S_OK;
}

//--------------------------------------------------------------------------------------
// DirectX11::Render()：DirectX関係の描画
//--------------------------------------------------------------------------------------
// Render() は毎フレーム呼ばれる関数です。ここに描画処理、入力処理、UI 表示などを記述します。
// このサンプルでは背景のクリア、円の描画、キーボード/ゲームパッド入力処理、テキスト描画、
// ゲームパッド振動といった処理を行っています。
void DirectX11::Render()
{
    auto& input = CInputManager::GetInstance(); // シングルトン取得

    // 毎フレームの入力状態更新
    input.Update();

    // レンダーターゲットを塗りつぶす
    m_D3DDeviceContext->ClearRenderTargetView(m_D3DRenderTargetView.Get(), DirectX::Colors::Aquamarine);

    //------------------------------------------------------------
    // 初期設定（円の位置・半径・速度）
    //------------------------------------------------------------
    static FLOAT fPosX1 = 200, fPosY1 = 300;
    static FLOAT fRadius1 = 50;
    static double dSpeed = 1.0;

    //------------------------------------------------------------
    // キー入力・ゲームパッド入力
    //------------------------------------------------------------
    bool keyA = input.IsKeyPress('A');
    bool keyD = input.IsKeyPress('D');
    bool keyW = input.IsKeyPress('W');
    bool keyS = input.IsKeyPress('S');

    // パッドボタン例（XINPUT_GAMEPAD_LEFT_SHOULDER 等）
    bool padLeft = input.IsPadPress(XINPUT_GAMEPAD_DPAD_LEFT);
    bool padRight = input.IsPadPress(XINPUT_GAMEPAD_DPAD_RIGHT);
    bool padUp = input.IsPadPress(XINPUT_GAMEPAD_DPAD_UP);
    bool padDown = input.IsPadPress(XINPUT_GAMEPAD_DPAD_DOWN);

    bool padA = input.IsPadPress(XINPUT_GAMEPAD_A);
    bool padB = input.IsPadPress(XINPUT_GAMEPAD_B);
    bool padX = input.IsPadPress(XINPUT_GAMEPAD_X);
    bool padY = input.IsPadPress(XINPUT_GAMEPAD_Y);
    bool padL = input.IsPadPress(XINPUT_GAMEPAD_LEFT_SHOULDER);
    bool padR = input.IsPadPress(XINPUT_GAMEPAD_RIGHT_SHOULDER);
    BYTE padZR = input.GetRightTrigger();
    BYTE padZL = input.GetLeftTrigger();

    float fThumbLX = input.GetThumbLX();
    float fThumbLY = input.GetThumbLY();

    //------------------------------------------------------------
    // 円の移動処理
    //------------------------------------------------------------
    if (fThumbLX != 0.0f || fThumbLY != 0.0f) // アナログスティック優先
    {
        fPosX1 += static_cast<FLOAT>(fThumbLX * Window::GetFrameTime() * dSpeed);
        fPosY1 -= static_cast<FLOAT>(fThumbLY * Window::GetFrameTime() * dSpeed);
    }
    else // キーボード or 十字キー
    {
        double dValue = 1;
        if ((keyA || keyD || padLeft || padRight) &&
            (keyW || keyS || padUp || padDown))
        {
            dValue = 1 / sqrt(2); // 斜め補正
        }

        if (keyA || padLeft)
            fPosX1 -= static_cast<FLOAT>(Window::GetFrameTime() * dSpeed * dValue);
        if (keyD || padRight)
            fPosX1 += static_cast<FLOAT>(Window::GetFrameTime() * dSpeed * dValue);
        if (keyW || padUp)
            fPosY1 -= static_cast<FLOAT>(Window::GetFrameTime() * dSpeed * dValue);
        if (keyS || padDown)
            fPosY1 += static_cast<FLOAT>(Window::GetFrameTime() * dSpeed * dValue);
    }

    // 画面端補正
    fPosX1 = (fPosX1 < fRadius1) ? fRadius1 : (fPosX1 > Window::GetClientWidth() - fRadius1 ? Window::GetClientWidth() - fRadius1 : fPosX1);
    fPosY1 = (fPosY1 < fRadius1) ? fRadius1 : (fPosY1 > Window::GetClientHeight() - fRadius1 ? Window::GetClientHeight() - fRadius1 : fPosY1);


    //------------------------------------------------------------
    // 振動設定（A/Bボタンで左右振動）
    //------------------------------------------------------------
    WORD leftMotor = padA ? 65535 : 0;
    WORD rightMotor = padB ? 65535 : 0;
    input.SetVibration(leftMotor, rightMotor);

    //------------------------------------------------------------
    // デバッグ文字列
    //------------------------------------------------------------
    WCHAR wcText1[256] = {};
    swprintf(wcText1, 256, L"FPS=%lf", Window::GetFps());

    WCHAR wcText2[256] = {};
    swprintf(wcText2, 256, L"A=%d D=%d W=%d S=%d", keyA, keyD, keyW, keyS);

    WCHAR wcText3[256] = {};
    swprintf(wcText3, 256, L"PAD_LEFT=%d PAD_RIGHT=%d PAD_UP=%d PAD_DOWN=%d", padLeft, padRight, padUp, padDown);

    WCHAR wcText4[256] = {};
    swprintf(wcText4, 256, L"PAD_A=%d PAD_B=%d PAD_X=%d PAD_Y=%d PAD_L=%d PAD_R=%d\n\n PAD_ZL=%d PAD_ZR=%d", padA, padB, padX, padY, padL, padR, padZL, padZR);

    WCHAR wcText5[256] = {};
    swprintf(wcText5, 256, L"sThumbLX=%f sThumbLY=%f", fThumbLX, fThumbLY);

    //------------------------------------------------------------
    // 2D描画
    //------------------------------------------------------------
    m_D2DDeviceContext->BeginDraw();
    m_D2DDeviceContext->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(fPosX1, fPosY1), fRadius1, fRadius1), m_D2DSolidBrush.Get(), 1);
    m_D2DDeviceContext->DrawText(wcText1, ARRAYSIZE(wcText1) - 1, m_DWriteTextFormat.Get(), D2D1::RectF(0, 0, 800, 20), m_D2DSolidBrush.Get());
    m_D2DDeviceContext->DrawText(wcText2, ARRAYSIZE(wcText2) - 1, m_DWriteTextFormat.Get(), D2D1::RectF(0, 20, 800, 40), m_D2DSolidBrush.Get());
    m_D2DDeviceContext->DrawText(wcText3, ARRAYSIZE(wcText3) - 1, m_DWriteTextFormat.Get(), D2D1::RectF(0, 40, 800, 60), m_D2DSolidBrush.Get());
    m_D2DDeviceContext->DrawText(wcText4, ARRAYSIZE(wcText4) - 1, m_DWriteTextFormat.Get(), D2D1::RectF(0, 60, 800, 80), m_D2DSolidBrush.Get());
    m_D2DDeviceContext->DrawText(wcText5, ARRAYSIZE(wcText5) - 1, m_DWriteTextFormat.Get(), D2D1::RectF(0, 80, 800, 100), m_D2DSolidBrush.Get());
    m_D2DDeviceContext->EndDraw();

    m_DXGISwapChain1->Present(0, 0);
}
