#include "../Common/SampleBase/SampleBase.h"

using namespace DirectX;

class HelloTriangle : public SampleBase
{
public:
    HelloTriangle( HINSTANCE hInstance );
    ~HelloTriangle();

    virtual bool Initialize() override;

private:
    virtual void OnResize() override;
    virtual void Update( const GameTimer& gt ) override;
    virtual void Draw( const GameTimer& gt ) override;
};

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd )
{
    // Enable run-time memory check for debug builds.
#if defined( DEBUG ) | defined( _DEBUG )
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    try
    {
        HelloTriangle theApp( hInstance );
        if ( !theApp.Initialize() )
            return 0;

        return theApp.Run();
    }
    catch ( DxException& e )
    {
        MessageBox( nullptr, e.ToString().c_str(), L"HR Failed", MB_OK );
        return 0;
    }
}


HelloTriangle::HelloTriangle( HINSTANCE hInstance ) :
    SampleBase( hInstance )
{
}

 HelloTriangle::~HelloTriangle()
{
}

bool HelloTriangle::Initialize()
{
    if ( !SampleBase::Initialize() )
        return false;

    return true;
}

void HelloTriangle::OnResize()
{
    SampleBase::OnResize();
}

void HelloTriangle::Update( const GameTimer& gt )
{
}

void HelloTriangle::Draw( const GameTimer& gt )
{
}
