/* === This file is part of Calamares - <https://github.com/calamares> ===
 *
 *   Copyright 2014-2015, Teo Mrnjavac <teo@kde.org>
 *   Copyright 2017-2018, Adriaan de Groot <groot@kde.org>
 *   Copyright 2018, Raul Rodrigo Segura (raurodse)
 *   Copyright 2019, Collabora Ltd <arnaud.ferraris@collabora.com>
 *
 *   Calamares is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Calamares is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Calamares. If not, see <http://www.gnu.org/licenses/>.
 */

#include "CalamaresWindow.h"

#include "Branding.h"
#include "DebugWindow.h"
#include "Settings.h"
#include "ViewManager.h"
#include "progresstree/ProgressTreeView.h"
#include "utils/CalamaresUtilsGui.h"
#include "utils/Logger.h"
#include "utils/Qml.h"
#include "utils/Retranslator.h"

#include <QApplication>
#include <QBoxLayout>
#include <QCloseEvent>
#include <QDesktopWidget>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QQuickWidget>
#include <QTreeView>

static inline int
windowDimensionToPixels( const Calamares::Branding::WindowDimension& u )
{
    if ( !u.isValid() )
    {
        return 0;
    }
    if ( u.unit() == Calamares::Branding::WindowDimensionUnit::Pixies )
    {
        return static_cast< int >( u.value() );
    }
    if ( u.unit() == Calamares::Branding::WindowDimensionUnit::Fonties )
    {
        return static_cast< int >( u.value() * CalamaresUtils::defaultFontHeight() );
    }
    return 0;
}


QWidget*
CalamaresWindow::getWidgetSidebar( int desiredWidth )
{
    const Calamares::Branding* const branding = Calamares::Branding::instance();

    QWidget* sideBox = new QWidget( this );
    sideBox->setObjectName( "sidebarApp" );

    QBoxLayout* sideLayout = new QVBoxLayout;
    sideBox->setLayout( sideLayout );
    // Set this attribute into qss file
    sideBox->setFixedWidth( desiredWidth );
    sideBox->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );

    QHBoxLayout* logoLayout = new QHBoxLayout;
    sideLayout->addLayout( logoLayout );
    logoLayout->addStretch();
    QLabel* logoLabel = new QLabel( sideBox );
    logoLabel->setObjectName( "logoApp" );
    //Define all values into qss file
    {
        QPalette plt = sideBox->palette();
        sideBox->setAutoFillBackground( true );
        plt.setColor( sideBox->backgroundRole(), branding->styleString( Calamares::Branding::SidebarBackground ) );
        plt.setColor( sideBox->foregroundRole(), branding->styleString( Calamares::Branding::SidebarText ) );
        sideBox->setPalette( plt );
        logoLabel->setPalette( plt );
    }
    logoLabel->setAlignment( Qt::AlignCenter );
    logoLabel->setFixedSize( 80, 80 );
    logoLabel->setPixmap( branding->image( Calamares::Branding::ProductLogo, logoLabel->size() ) );
    logoLayout->addWidget( logoLabel );
    logoLayout->addStretch();

    ProgressTreeView* tv = new ProgressTreeView( sideBox );
    tv->setModel( Calamares::ViewManager::instance() );
    tv->setFocusPolicy( Qt::NoFocus );
    sideLayout->addWidget( tv );

    if ( Calamares::Settings::instance()->debugMode() || ( Logger::logLevel() >= Logger::LOGVERBOSE ) )
    {
        QPushButton* debugWindowBtn = new QPushButton;
        debugWindowBtn->setObjectName( "debugButton" );
        CALAMARES_RETRANSLATE( debugWindowBtn->setText( tr( "Show debug information" ) ); )
        sideLayout->addWidget( debugWindowBtn );
        debugWindowBtn->setFlat( true );
        debugWindowBtn->setCheckable( true );
        connect( debugWindowBtn, &QPushButton::clicked, this, [=]( bool checked ) {
            if ( checked )
            {
                m_debugWindow = new Calamares::DebugWindow();
                m_debugWindow->show();
                connect( m_debugWindow.data(), &Calamares::DebugWindow::closed, this, [=]() {
                    m_debugWindow->deleteLater();
                    debugWindowBtn->setChecked( false );
                } );
            }
            else
            {
                if ( m_debugWindow )
                {
                    m_debugWindow->deleteLater();
                }
            }
        } );
    }

    CalamaresUtils::unmarginLayout( sideLayout );
    return sideBox;
}

QWidget*
CalamaresWindow::getQmlSidebar( int )
{
    CalamaresUtils::registerCalamaresModels();
    QQuickWidget* w = new QQuickWidget( this );
    w->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    w->setResizeMode( QQuickWidget::SizeRootObjectToView );
    w->setSource( QUrl(
        CalamaresUtils::searchQmlFile( CalamaresUtils::QmlSearch::Both, QStringLiteral( "calamares-sidebar" ) ) ) );
    return w;
}

/** @brief Get a button-sized icon. */
static inline QPixmap
getButtonIcon( const QString& name )
{
    return Calamares::Branding::instance()->image( name, QSize( 22, 22 ) );
}

static inline void
setButtonIcon( QPushButton* button, const QString& name )
{
    auto icon = getButtonIcon( name );
    if ( button && !icon.isNull() )
    {
        button->setIcon( icon );
    }
}

QWidget*
CalamaresWindow::getWidgetNavigation()
{
    QWidget* navigation = new QWidget( this );
    QBoxLayout* bottomLayout = new QHBoxLayout;
    bottomLayout->addStretch();

    // Create buttons and sets an initial icon; the icons may change
    {
        auto* back = new QPushButton( getButtonIcon( QStringLiteral( "go-previous" ) ), tr( "&Back" ), navigation );
        back->setObjectName( "view-button-back" );
        back->setEnabled( m_viewManager->backEnabled() );
        connect( back, &QPushButton::clicked, m_viewManager, &Calamares::ViewManager::back );
        connect( m_viewManager, &Calamares::ViewManager::backEnabledChanged, back, &QPushButton::setEnabled );
        connect( m_viewManager, &Calamares::ViewManager::backLabelChanged, back, &QPushButton::setText );
        connect( m_viewManager, &Calamares::ViewManager::backIconChanged, this, [=]( QString n ) {
            setButtonIcon( back, n );
        } );
        bottomLayout->addWidget( back );
    }
    {
        auto* next = new QPushButton( getButtonIcon( QStringLiteral( "go-next" ) ), tr( "&Next" ), navigation );
        next->setObjectName( "view-button-next" );
        next->setEnabled( m_viewManager->nextEnabled() );
        connect( next, &QPushButton::clicked, m_viewManager, &Calamares::ViewManager::next );
        connect( m_viewManager, &Calamares::ViewManager::nextEnabledChanged, next, &QPushButton::setEnabled );
        connect( m_viewManager, &Calamares::ViewManager::nextLabelChanged, next, &QPushButton::setText );
        connect( m_viewManager, &Calamares::ViewManager::nextIconChanged, this, [=]( QString n ) {
            setButtonIcon( next, n );
        } );
        bottomLayout->addWidget( next );
    }
    bottomLayout->addSpacing( 12 );
    {
        auto* quit = new QPushButton( getButtonIcon( QStringLiteral( "dialog-cancel" ) ), tr( "&Cancel" ), navigation );
        quit->setObjectName( "view-button-cancel" );
        connect( quit, &QPushButton::clicked, m_viewManager, &Calamares::ViewManager::quit );
        connect( m_viewManager, &Calamares::ViewManager::quitEnabledChanged, quit, &QPushButton::setEnabled );
        connect( m_viewManager, &Calamares::ViewManager::quitLabelChanged, quit, &QPushButton::setText );
        connect( m_viewManager, &Calamares::ViewManager::quitIconChanged, this, [=]( QString n ) {
            setButtonIcon( quit, n );
        } );
        connect( m_viewManager, &Calamares::ViewManager::quitTooltipChanged, quit, &QPushButton::setToolTip );
        connect( m_viewManager, &Calamares::ViewManager::quitVisibleChanged, quit, &QPushButton::setVisible );
        bottomLayout->addWidget( quit );
    }

    navigation->setLayout( bottomLayout );
    return navigation;
}

QWidget*
CalamaresWindow::getQmlNavigation()
{
    CalamaresUtils::registerCalamaresModels();
    QQuickWidget* w = new QQuickWidget( this );
    w->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
    w->setResizeMode( QQuickWidget::SizeRootObjectToView );
    w->setSource( QUrl(
        CalamaresUtils::searchQmlFile( CalamaresUtils::QmlSearch::Both, QStringLiteral( "calamares-navigation" ) ) ) );
    return w;
}

/**@brief Picks one of two methods to call
 *
 * Calls method (member function) @p widget or @p qml with arguments @p a
 * on the given window, based on the flavor.
 */
template < typename widgetMaker, typename... args >
QWidget*
flavoredWidget( Calamares::Branding::PanelFlavor flavor,
                CalamaresWindow* w,
                widgetMaker widget,
                widgetMaker qml,
                args... a )
{
    // Member-function calling syntax is (object.*member)(args)
    switch ( flavor )
    {
    case Calamares::Branding::PanelFlavor::Widget:
        return ( w->*widget )( a... );
    case Calamares::Branding::PanelFlavor::Qml:
        return ( w->*qml )( a... );
    case Calamares::Branding::PanelFlavor::None:
        return nullptr;
    }
    NOTREACHED return nullptr;  // All enum values handled above
}

/** @brief Adds widgets to @p layout if they belong on this @p side
 */
static inline void
insertIf( QBoxLayout* layout,
          Calamares::Branding::PanelSide side,
          QWidget* first,
          Calamares::Branding::PanelSide firstSide )
{
    if ( first && side == firstSide )
    {
        if ( ( side == Calamares::Branding::PanelSide::Left ) || ( side == Calamares::Branding::PanelSide::Right ) )
        {
            first->setMinimumWidth( qMax( first->minimumWidth(), 64 ) );
        }
        else
        {
            first->setMinimumHeight( qMax( first->minimumHeight(), 64 ) );
        }
        layout->addWidget( first );
    }
}

CalamaresWindow::CalamaresWindow( QWidget* parent )
    : QWidget( parent )
    , m_debugWindow( nullptr )
    , m_viewManager( nullptr )
{
    // If we can never cancel, don't show the window-close button
    if ( Calamares::Settings::instance()->disableCancel() )
    {
        setWindowFlag( Qt::WindowCloseButtonHint, false );
    }

    CALAMARES_RETRANSLATE( setWindowTitle( Calamares::Settings::instance()->isSetupMode()
                                               ? tr( "%1 Setup Program" ).arg( *Calamares::Branding::ProductName )
                                               : tr( "%1 Installer" ).arg( *Calamares::Branding::ProductName ) ); )

    const Calamares::Branding* const branding = Calamares::Branding::instance();

    using CalamaresUtils::windowMinimumHeight;
    using CalamaresUtils::windowMinimumWidth;
    using CalamaresUtils::windowPreferredHeight;
    using CalamaresUtils::windowPreferredWidth;

    using PanelSide = Calamares::Branding::PanelSide;

    // Needs to match what's checked in DebugWindow
    this->setObjectName( "mainApp" );

    QSize availableSize = qApp->desktop()->availableGeometry( this ).size();
    QSize minimumSize( qBound( windowMinimumWidth, availableSize.width(), windowPreferredWidth ),
                       qBound( windowMinimumHeight, availableSize.height(), windowPreferredHeight ) );
    setMinimumSize( minimumSize );

    cDebug() << "Available desktop" << availableSize << "minimum size" << minimumSize;

    auto brandingSizes = branding->windowSize();

    int w = qBound( minimumSize.width(), windowDimensionToPixels( brandingSizes.first ), availableSize.width() );
    int h = qBound( minimumSize.height(), windowDimensionToPixels( brandingSizes.second ), availableSize.height() );

    cDebug() << Logger::SubEntry << "Proposed window size:" << w << h;
    resize( w, h );

    m_viewManager = Calamares::ViewManager::instance( this );
    if ( branding->windowExpands() )
    {
        connect( m_viewManager, &Calamares::ViewManager::enlarge, this, &CalamaresWindow::enlarge );
    }
    // NOTE: Although the ViewManager has a signal cancelEnabled() that
    //       signals when the state of the cancel button changes (in
    //       particular, to disable cancel during the exec phase),
    //       we don't connect to it here. Changing the window flag
    //       for the close button causes uncomfortable window flashing
    //       and requires an extra show() (at least with KWin/X11) which
    //       is too annoying. Instead, leave it up to ignoring-the-quit-
    //       event, which is also the ViewManager's responsibility.

    QBoxLayout* mainLayout = new QHBoxLayout;
    QBoxLayout* contentsLayout = new QVBoxLayout;

    setLayout( mainLayout );

    QWidget* sideBox = flavoredWidget(
        branding->sidebarFlavor(),
        this,
        &CalamaresWindow::getWidgetSidebar,
        &CalamaresWindow::getQmlSidebar,
        qBound( 100, CalamaresUtils::defaultFontHeight() * 12, w < windowPreferredWidth ? 100 : 190 ) );
    QWidget* navigation = flavoredWidget(
        branding->navigationFlavor(), this, &CalamaresWindow::getWidgetNavigation, &CalamaresWindow::getQmlNavigation );

    // Build up the contentsLayout (a VBox) top-to-bottom
    // .. note that the bottom is mirrored wrt. the top
    insertIf( contentsLayout, PanelSide::Top, sideBox, branding->sidebarSide() );
    insertIf( contentsLayout, PanelSide::Top, navigation, branding->navigationSide() );
    contentsLayout->addWidget( m_viewManager->centralWidget() );
    insertIf( contentsLayout, PanelSide::Bottom, navigation, branding->navigationSide() );
    insertIf( contentsLayout, PanelSide::Bottom, sideBox, branding->sidebarSide() );

    // .. and then the mainLayout left-to-right
    insertIf( mainLayout, PanelSide::Left, sideBox, branding->sidebarSide() );
    insertIf( mainLayout, PanelSide::Left, navigation, branding->navigationSide() );
    mainLayout->addLayout( contentsLayout );
    insertIf( mainLayout, PanelSide::Right, navigation, branding->navigationSide() );
    insertIf( mainLayout, PanelSide::Right, sideBox, branding->sidebarSide() );

    CalamaresUtils::unmarginLayout( mainLayout );
    CalamaresUtils::unmarginLayout( contentsLayout );
    setStyleSheet( Calamares::Branding::instance()->stylesheet() );
}

void
CalamaresWindow::enlarge( QSize enlarge )
{
    auto mainGeometry = this->geometry();
    QSize availableSize = qApp->desktop()->availableGeometry( this ).size();

    auto h = qBound( 0, mainGeometry.height() + enlarge.height(), availableSize.height() );
    auto w = this->size().width();

    resize( w, h );
}

void
CalamaresWindow::closeEvent( QCloseEvent* event )
{
    if ( ( !m_viewManager ) || m_viewManager->confirmCancelInstallation() )
    {
        event->accept();
        qApp->quit();
    }
    else
    {
        event->ignore();
    }
}
