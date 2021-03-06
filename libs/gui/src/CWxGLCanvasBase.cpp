/* +------------------------------------------------------------------------+
   |                     Mobile Robot Programming Toolkit (MRPT)            |
   |                          http://www.mrpt.org/                          |
   |                                                                        |
   | Copyright (c) 2005-2017, Individual contributors, see AUTHORS file     |
   | See: http://www.mrpt.org/Authors - All rights reserved.                |
   | Released under BSD License. See details in http://www.mrpt.org/License |
   +------------------------------------------------------------------------+ */

#include "gui-precomp.h"  // Precompiled headers

#include <mrpt/gui/CWxGLCanvasBase.h>
#include <mrpt/gui/WxSubsystem.h>
#include <mrpt/utils/CTicTac.h>

#if MRPT_HAS_WXWIDGETS && MRPT_HAS_OPENGL_GLUT

using namespace mrpt;
using namespace mrpt::utils;
using namespace mrpt::gui;
using namespace mrpt::opengl;
using namespace std;

#if MRPT_HAS_OPENGL_GLUT
#ifdef MRPT_OS_WINDOWS
// Windows:
#include <windows.h>
#endif

#ifdef MRPT_OS_APPLE
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <GLUT/glut.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#ifdef HAVE_FREEGLUT_EXT_H
#include <GL/freeglut_ext.h>
#endif
#endif

#endif

#if !wxUSE_GLCANVAS
#error "OpenGL required: set wxUSE_GLCANVAS to 1 and rebuild wxWidgets"
#endif

/*----------------------------------------------------------------
  Implementation of Test-GLCanvas
-----------------------------------------------------------------*/

BEGIN_EVENT_TABLE(CWxGLCanvasBase, wxGLCanvas)
EVT_SIZE(CWxGLCanvasBase::OnSize)
EVT_PAINT(CWxGLCanvasBase::OnPaint)
EVT_ERASE_BACKGROUND(CWxGLCanvasBase::OnEraseBackground)
EVT_ENTER_WINDOW(CWxGLCanvasBase::OnEnterWindow)
EVT_WINDOW_CREATE(CWxGLCanvasBase::OnWindowCreation)
END_EVENT_TABLE()

void CWxGLCanvasBase::OnWindowCreation(wxWindowCreateEvent& ev)
{
	if (!m_gl_context) m_gl_context = new wxGLContext(this);
}

void CWxGLCanvasBase::swapBuffers() { SwapBuffers(); }
void CWxGLCanvasBase::preRender() { OnPreRender(); }
void CWxGLCanvasBase::postRender() { OnPostRender(); }
void CWxGLCanvasBase::renderError(const string& err_msg)
{
	OnRenderError(_U(err_msg.c_str()));
}

void CWxGLCanvasBase::OnMouseDown(wxMouseEvent& event)
{
	setMousePos(event.GetX(), event.GetY());
	setMouseClicked(true);
}
void CWxGLCanvasBase::OnMouseUp(wxMouseEvent& /*event*/)
{
	setMouseClicked(false);
}

void CWxGLCanvasBase::OnMouseMove(wxMouseEvent& event)
{
	bool leftIsDown = event.LeftIsDown();

	if (leftIsDown || event.RightIsDown())
	{
		int X = event.GetX();
		int Y = event.GetY();
		updateLastPos(X, Y);

		// Proxy variables to cache the changes:
		CamaraParams params = cameraParams();

		if (leftIsDown)
		{
			if (event.ShiftDown())
				updateZoom(params, X, Y);

			else if (event.ControlDown())
				updateRotate(params, X, Y);

			else
				updateOrbitCamera(params, X, Y);
		}
		else
			updatePan(params, X, Y);

		setMousePos(X, Y);
		setCameraParams(params);

#if wxCHECK_VERSION(2, 9, 5)
		wxTheApp->SafeYieldFor(nullptr, wxEVT_CATEGORY_TIMER);
#endif
		Refresh(false);
	}

	// ensure we have the focus so we get keyboard events:
	// this->SetFocus(); // JLBC: Commented out to avoid a crash after 
	// refactor with Qt
}

void CWxGLCanvasBase::OnMouseWheel(wxMouseEvent& event)
{
	CamaraParams params = cameraParams();
	updateZoom(params, event.GetWheelRotation());
	setCameraParams(params);

	Refresh(false);
	// ensure we have the focus so we get keyboard events:
	this->SetFocus();
}

static int WX_GL_ATTR_LIST[] = {WX_GL_DOUBLEBUFFER, WX_GL_RGBA,
								WX_GL_DEPTH_SIZE, 24, 0};

CWxGLCanvasBase::CWxGLCanvasBase(
	wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size,
	long style, const wxString& name)
	: CGlCanvasBase(),
	  wxGLCanvas(
		  parent, id, WX_GL_ATTR_LIST, pos, size,
		  style | wxFULL_REPAINT_ON_RESIZE, name),
	  m_gl_context(nullptr),
	  m_init(false)
{
	Connect(
		wxID_ANY, wxEVT_LEFT_DOWN,
		(wxObjectEventFunction)&CWxGLCanvasBase::OnMouseDown);
	Connect(
		wxID_ANY, wxEVT_RIGHT_DOWN,
		(wxObjectEventFunction)&CWxGLCanvasBase::OnMouseDown);
	Connect(
		wxID_ANY, wxEVT_LEFT_UP,
		(wxObjectEventFunction)&CWxGLCanvasBase::OnMouseUp);
	Connect(
		wxID_ANY, wxEVT_RIGHT_UP,
		(wxObjectEventFunction)&CWxGLCanvasBase::OnMouseUp);
	Connect(
		wxID_ANY, wxEVT_MOTION,
		(wxObjectEventFunction)&CWxGLCanvasBase::OnMouseMove);
	Connect(
		wxID_ANY, wxEVT_MOUSEWHEEL,
		(wxObjectEventFunction)&CWxGLCanvasBase::OnMouseWheel);

	Connect(
		wxID_ANY, wxEVT_CHAR, (wxObjectEventFunction)&CWxGLCanvasBase::OnChar);
	Connect(
		wxID_ANY, wxEVT_CHAR_HOOK,
		(wxObjectEventFunction)&CWxGLCanvasBase::OnChar);

	Connect(
		wxEVT_CREATE,
		(wxObjectEventFunction)&CWxGLCanvasBase::OnWindowCreation);

// JL: There seems to be a problem in MSW we don't receive this event, but
//      in GTK we do and at the right moment to avoid an X server crash.
#ifdef _WIN32
	wxWindowCreateEvent dum;
	OnWindowCreation(dum);
#endif
}

CWxGLCanvasBase::~CWxGLCanvasBase() { delete_safe(m_gl_context); }
void CWxGLCanvasBase::OnChar(wxKeyEvent& event) { OnCharCustom(event); }
void CWxGLCanvasBase::Render()
{
	wxPaintDC dc(this);

	if (!m_gl_context)
	{ /*cerr << "[CWxGLCanvasBase::Render] No GL Context!" << endl;*/
		return;
	}
	else
		SetCurrent(*m_gl_context);

	// Init OpenGL once, but after SetCurrent
	if (!m_init)
	{
		InitGL();
		m_init = true;
	}

	int width, height;
	GetClientSize(&width, &height);
	double At = renderCanvas(width, height);

	OnPostRenderSwapBuffers(At, dc);
}

void CWxGLCanvasBase::OnEnterWindow(wxMouseEvent& WXUNUSED(event))
{
	SetFocus();
}

void CWxGLCanvasBase::OnPaint(wxPaintEvent& WXUNUSED(event)) { Render(); }
void CWxGLCanvasBase::OnSize(wxSizeEvent& event)
{
	if (!m_parent->IsShown()) return;

	// set GL viewport (not called by wxGLCanvas::OnSize on all platforms...)
	int w, h;
	GetClientSize(&w, &h);

	if (this->IsShownOnScreen())
	{
		if (!m_gl_context)
		{ /*cerr << "[CWxGLCanvasBase::Render] No GL Context!" << endl;*/
			return;
		}
		else
			SetCurrent(*m_gl_context);

		resizeViewport(w, h);
	}
}

void CWxGLCanvasBase::OnEraseBackground(wxEraseEvent& WXUNUSED(event))
{
	// Do nothing, to avoid flashing.
}

void CWxGLCanvasBase::InitGL()
{
	if (!m_gl_context)
	{ /*cerr << "[CWxGLCanvasBase::Render] No GL Context!" << endl;*/
		return;
	}
	else
		SetCurrent(*m_gl_context);

	static bool GLUT_INIT_DONE = false;

	if (!GLUT_INIT_DONE)
	{
		GLUT_INIT_DONE = true;

		int argc = 1;
		char* argv[1] = {nullptr};
		glutInit(&argc, argv);
	}
}

void CWxGLCanvasBase::setCameraPose(const mrpt::poses::CPose3D& camPose)
{
	THROW_EXCEPTION("todo")
}

#endif  // MRPT_HAS_WXWIDGETS
