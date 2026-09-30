/***************************************************************************
  qgsappribbon.cpp
  -------------------
  begin                : August 2026
  copyright            : (C) 2026 by Hake Technologies
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsappribbon.h"

#include "qgisapp.h"

#include <algorithm>
#include <array>
#include <utility>

#include <QAction>
#include <QDockWidget>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSizePolicy>
#include <QStyle>
#include <QTabBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QWidgetAction>

#include "moc_qgsappribbon.cpp"

using namespace Qt::StringLiterals;

namespace
{
  QFont ribbonCaptionFont( const QFont &base )
  {
    QFont f = base;
    if ( f.pointSizeF() > 0 )
      f.setPointSizeF( f.pointSizeF() * 0.85 );
    else
      f.setPixelSize( std::max( 8, qRound( f.pixelSize() * 0.85 ) ) );
    return f;
  }

  bool isPresentableAction( const QAction *action )
  {
    return action && !action->isSeparator() && !qobject_cast<const QWidgetAction *>( action );
  }

  //! The menu behind a QMenu::menuAction() drop-down, as opposed to a regular command that merely carries a menu.
  QMenu *dropDownMenu( const QAction *action )
  {
    QMenu *menu = action ? action->menu() : nullptr;
    return menu && menu->menuAction() == action ? menu : nullptr;
  }

  //! Visible, and for a menu drop-down, the menu has at least one visible command.
  bool isShowableCommand( const QAction *action )
  {
    if ( !action || !action->isVisible() )
      return false;
    if ( const QMenu *menu = dropDownMenu( action ) )
    {
      const QList<QAction *> actions = menu->actions();
      return std::any_of( actions.cbegin(), actions.cend(), []( const QAction *a ) { return !a->isSeparator() && a->isVisible(); } );
    }
    return true;
  }

  //! Toolbar widgets such as drop-down tool buttons are presented through their default action.
  QAction *presentableToolbarAction( QAction *action )
  {
    if ( auto *widgetAction = qobject_cast<QWidgetAction *>( action ) )
    {
      auto *button = qobject_cast<QToolButton *>( widgetAction->defaultWidget() );
      return button ? button->defaultAction() : nullptr;
    }
    return action;
  }
} // namespace

// Group caption that elides instead of forcing its group wider than its buttons.
// Color comes from the theme QSS (#HakeAppRibbonGroupCaption) via the palette.
class QgsAppRibbonCaption : public QWidget
{
  public:
    QgsAppRibbonCaption( const QString &text, QWidget *parent )
      : QWidget( parent )
      , mText( text )
    {
      setObjectName( u"HakeAppRibbonGroupCaption"_s );
      setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Fixed );
      setAttribute( Qt::WA_TransparentForMouseEvents, true );
    }

    QSize sizeHint() const override
    {
      const QFontMetrics fm( font() );
      return QSize( fm.horizontalAdvance( mText ) + fm.averageCharWidth() * 2, fm.height() );
    }

    QSize minimumSizeHint() const override { return QSize( 0, QFontMetrics( font() ).height() ); }

  protected:
    void paintEvent( QPaintEvent * ) override
    {
      QPainter p( this );
      p.setPen( palette().color( QPalette::WindowText ) );
      const QString shown = fontMetrics().elidedText( mText, Qt::ElideRight, width() );
      p.drawText( rect(), Qt::AlignHCenter | Qt::AlignVCenter, shown );
    }

  private:
    QString mText;
};

class QgsAppRibbonGroup : public QWidget
{
  public:
    enum Level
    {
      Full,
      Compact,
      IconsOnly,
      Collapsed,
    };

    struct Entry
    {
        QPointer<QAction> action;
        bool primary = false;
        //! Dock whose toggleViewAction() fills this entry once the dock exists
        QString dockObjectName;
        //! Menu whose menuAction() fills this entry once the menu exists
        QString menuObjectName;
    };

    QgsAppRibbonGroup( const QString &title, QWidget *parent );

    QString title() const { return mTitle; }
    void addEntry( const Entry &entry );
    void setEntries( const QList<Entry> &entries );
    bool resolveDeferred( QObject *root );
    QList<QAction *> commands() const;
    bool hasCommands() const { return !commands().isEmpty(); }

    void setMetrics( const QgsAppRibbonMetrics &metrics );
    void rebuild();
    void scheduleRebuild();

    Level level() const { return mLevel; }
    void setLevel( Level level );
    int widthForLevel( Level level ) const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

  protected:
    bool eventFilter( QObject *watched, QEvent *event ) override;

  private:
    void watchAction( QAction *action );
    QWidget *buildVariant( Level level );
    QToolButton *makeButton( QWidget *parent, QAction *action, Qt::ToolButtonStyle style, int iconSize ) const;

    QString mTitle;
    QList<Entry> mEntries;
    QgsAppRibbonMetrics mMetrics;
    Level mLevel = Full;
    QWidget *mContent = nullptr;
    QHBoxLayout *mContentLayout = nullptr;
    QgsAppRibbonCaption *mCaption = nullptr;
    std::array<QWidget *, 4> mVariants { { nullptr, nullptr, nullptr, nullptr } };
    bool mRebuildPending = false;
};

class QgsAppRibbonPage : public QWidget
{
  public:
    explicit QgsAppRibbonPage( QWidget *parent );

    QgsAppRibbonGroup *addGroup( const QString &title );
    const QList<QgsAppRibbonGroup *> &groups() const { return mGroups; }
    void setMetrics( const QgsAppRibbonMetrics &metrics );
    void relayout();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

  protected:
    void resizeEvent( QResizeEvent *event ) override;
    void showEvent( QShowEvent *event ) override;

  private:
    int requiredWidth( const QVector<QgsAppRibbonGroup::Level> &levels, const QVector<bool> &hidden, bool overflow ) const;

    QHBoxLayout *mLayout = nullptr;
    QList<QgsAppRibbonGroup *> mGroups;
    QList<QFrame *> mSeparators;
    QToolButton *mOverflow = nullptr;
    QMenu *mOverflowMenu = nullptr;
    bool mInRelayout = false;
};

//
// QgsAppRibbonGroup
//

QgsAppRibbonGroup::QgsAppRibbonGroup( const QString &title, QWidget *parent )
  : QWidget( parent )
  , mTitle( title )
{
  setAccessibleName( title );
  setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Expanding );

  auto *layout = new QVBoxLayout( this );
  layout->setContentsMargins( 0, 0, 0, 0 );
  layout->setSpacing( 0 );

  mContent = new QWidget( this );
  mContentLayout = new QHBoxLayout( mContent );
  mContentLayout->setContentsMargins( 0, 0, 0, 0 );
  mContentLayout->setSpacing( 0 );
  layout->addWidget( mContent, 1 );

  mCaption = new QgsAppRibbonCaption( title, this );
  layout->addWidget( mCaption, 0 );
}

void QgsAppRibbonGroup::watchAction( QAction *action )
{
  if ( !action )
    return;
  connect( action, &QAction::visibleChanged, this, &QgsAppRibbonGroup::scheduleRebuild, Qt::UniqueConnection );
  // Menu drop-downs appear/disappear as plugins populate or empty the menu.
  if ( QMenu *menu = dropDownMenu( action ) )
    menu->installEventFilter( this );
}

bool QgsAppRibbonGroup::eventFilter( QObject *watched, QEvent *event )
{
  if ( ( event->type() == QEvent::ActionAdded || event->type() == QEvent::ActionRemoved ) && qobject_cast<QMenu *>( watched ) )
    scheduleRebuild();
  return QWidget::eventFilter( watched, event );
}

void QgsAppRibbonGroup::addEntry( const Entry &entry )
{
  if ( entry.action && !isPresentableAction( entry.action ) )
    return;
  if ( !entry.action && entry.dockObjectName.isEmpty() && entry.menuObjectName.isEmpty() )
    return;
  mEntries.append( entry );
  watchAction( entry.action );
}

void QgsAppRibbonGroup::setEntries( const QList<Entry> &entries )
{
  mEntries.clear();
  for ( const Entry &entry : entries )
    addEntry( entry );
  scheduleRebuild();
}

bool QgsAppRibbonGroup::resolveDeferred( QObject *root )
{
  bool resolved = false;
  for ( Entry &entry : mEntries )
  {
    if ( entry.action )
      continue;
    if ( !entry.dockObjectName.isEmpty() )
    {
      if ( QDockWidget *dock = root->findChild<QDockWidget *>( entry.dockObjectName ) )
        entry.action = dock->toggleViewAction();
    }
    else if ( !entry.menuObjectName.isEmpty() )
    {
      if ( QMenu *menu = root->findChild<QMenu *>( entry.menuObjectName ) )
        entry.action = menu->menuAction();
    }
    if ( entry.action )
    {
      watchAction( entry.action );
      resolved = true;
    }
  }
  return resolved;
}

QList<QAction *> QgsAppRibbonGroup::commands() const
{
  QList<QAction *> list;
  for ( const Entry &entry : mEntries )
  {
    if ( isShowableCommand( entry.action ) )
      list << entry.action;
  }
  return list;
}

void QgsAppRibbonGroup::setMetrics( const QgsAppRibbonMetrics &metrics )
{
  mMetrics = metrics;
  rebuild();
}

void QgsAppRibbonGroup::scheduleRebuild()
{
  if ( mRebuildPending )
    return;
  mRebuildPending = true;
  QTimer::singleShot( 0, this, [this] {
    mRebuildPending = false;
    rebuild();
    if ( auto *page = dynamic_cast<QgsAppRibbonPage *>( parentWidget() ) )
      page->relayout();
  } );
}

void QgsAppRibbonGroup::rebuild()
{
  // deleteLater: a rebuild can be triggered while one of these buttons is still
  // inside its own click handler (e.g. an action that changes the theme).
  for ( QWidget *&variant : mVariants )
  {
    if ( variant )
    {
      variant->hide();
      variant->deleteLater();
      variant = nullptr;
    }
  }

  mCaption->setFont( ribbonCaptionFont( font() ) );
  for ( int l = Full; l <= Collapsed; ++l )
  {
    mVariants[l] = buildVariant( static_cast<Level>( l ) );
    mContentLayout->addWidget( mVariants[l], 0, Qt::AlignLeft | Qt::AlignVCenter );
    mVariants[l]->ensurePolished();
  }
  setLevel( mLevel );
}

QToolButton *QgsAppRibbonGroup::makeButton( QWidget *parent, QAction *action, Qt::ToolButtonStyle style, int iconSize ) const
{
  auto *button = new QToolButton( parent );
  button->setDefaultAction( action );
  button->setAutoRaise( true );
  button->setToolButtonStyle( style );
  button->setIconSize( QSize( iconSize, iconSize ) );
  // Reachable by keyboard, but a mouse click must not steal focus from the map canvas.
  button->setFocusPolicy( Qt::TabFocus );
  button->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
  if ( dropDownMenu( action ) )
    button->setPopupMode( QToolButton::InstantPopup );
  return button;
}

QWidget *QgsAppRibbonGroup::buildVariant( Level level )
{
  auto *variant = new QWidget( mContent );
  auto *grid = new QGridLayout( variant );
  grid->setContentsMargins( 0, 0, 0, 0 );
  grid->setHorizontalSpacing( 2 );
  grid->setVerticalSpacing( 0 );

  const QgsAppRibbonMetrics &m = mMetrics;
  const int tallButtonHeight = std::min( m.buttonAreaHeight, std::max( m.tallHeight, m.rows * m.rowHeight ) );

  QList<Entry> visibleEntries;
  for ( const Entry &entry : std::as_const( mEntries ) )
  {
    if ( isShowableCommand( entry.action ) )
      visibleEntries << entry;
  }
  if ( visibleEntries.isEmpty() )
    return variant;

  if ( level == Collapsed )
  {
    auto *button = new QToolButton( variant );
    button->setText( mTitle + u" \u25BE"_s );
    button->setToolTip( mTitle );
    button->setAccessibleName( mTitle );
    button->setIcon( visibleEntries.first().action->icon() );
    button->setAutoRaise( true );
    button->setFocusPolicy( Qt::TabFocus );
    button->setPopupMode( QToolButton::InstantPopup );
    auto *menu = new QMenu( button );
    menu->addActions( commands() );
    button->setMenu( menu );
    if ( m.tallPrimary )
    {
      button->setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
      button->setIconSize( QSize( m.largeIcon, m.largeIcon ) );
      button->setFixedHeight( tallButtonHeight );
    }
    else
    {
      button->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
      button->setIconSize( QSize( m.smallIcon, m.smallIcon ) );
      button->setFixedHeight( m.rowHeight );
    }
    grid->addWidget( button, 0, 0, Qt::AlignLeft | Qt::AlignVCenter );
    return variant;
  }

  int row = 0;
  int col = 0;
  for ( const Entry &entry : std::as_const( visibleEntries ) )
  {
    const bool tall = entry.primary && m.tallPrimary && level != IconsOnly;
    if ( tall )
    {
      if ( row != 0 )
      {
        ++col;
        row = 0;
      }
      QToolButton *button = makeButton( variant, entry.action, Qt::ToolButtonTextUnderIcon, m.largeIcon );
      button->setFixedHeight( tallButtonHeight );
      grid->addWidget( button, 0, col, m.rows, 1, Qt::AlignLeft | Qt::AlignVCenter );
      ++col;
      continue;
    }

    Qt::ToolButtonStyle style = Qt::ToolButtonIconOnly;
    if ( level == Full || ( level == Compact && entry.primary ) )
      style = Qt::ToolButtonTextBesideIcon;
    // Menu-only commands may have no icon; an icon-only button would be blank.
    if ( entry.action->icon().isNull() )
      style = Qt::ToolButtonTextOnly;

    QToolButton *button = makeButton( variant, entry.action, style, m.smallIcon );
    button->setFixedHeight( m.rowHeight );
    grid->addWidget( button, row, col, Qt::AlignLeft | Qt::AlignVCenter );
    if ( ++row >= m.rows )
    {
      row = 0;
      ++col;
    }
  }
  return variant;
}

void QgsAppRibbonGroup::setLevel( Level level )
{
  mLevel = level;
  for ( int l = Full; l <= Collapsed; ++l )
  {
    if ( mVariants[l] )
      mVariants[l]->setVisible( l == level );
  }
  mCaption->setVisible( mMetrics.captions && level != Collapsed );
  updateGeometry();
}

int QgsAppRibbonGroup::widthForLevel( Level level ) const
{
  const QWidget *variant = mVariants[level];
  if ( !variant )
    return 0;
  const int content = variant->sizeHint().width();
  if ( !mMetrics.captions || level == Collapsed )
    return content;
  const int caption = mCaption->sizeHint().width();
  if ( level == IconsOnly )
    return std::max( content, std::min( caption, mCaption->fontMetrics().averageCharWidth() * 6 ) );
  return std::max( content, caption );
}

QSize QgsAppRibbonGroup::sizeHint() const
{
  return QSize( widthForLevel( mLevel ), QWidget::sizeHint().height() );
}

QSize QgsAppRibbonGroup::minimumSizeHint() const
{
  return QSize( widthForLevel( mLevel ), 0 );
}

//
// QgsAppRibbonPage
//

QgsAppRibbonPage::QgsAppRibbonPage( QWidget *parent )
  : QWidget( parent )
{
  setObjectName( u"HakeAppRibbonPage"_s );
  setAttribute( Qt::WA_StyledBackground, true );
  setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Ignored );

  mLayout = new QHBoxLayout( this );
  mLayout->setSpacing( 4 );
  mLayout->addStretch( 1 );

  mOverflowMenu = new QMenu( this );
  mOverflow = new QToolButton( this );
  mOverflow->setText( u"\u00BB"_s );
  mOverflow->setToolTip( QObject::tr( "More commands" ) );
  mOverflow->setAccessibleName( QObject::tr( "More commands" ) );
  mOverflow->setToolButtonStyle( Qt::ToolButtonTextOnly );
  mOverflow->setAutoRaise( true );
  mOverflow->setFocusPolicy( Qt::TabFocus );
  mOverflow->setPopupMode( QToolButton::InstantPopup );
  mOverflow->setMenu( mOverflowMenu );
  mOverflow->hide();
  mLayout->addWidget( mOverflow, 0, Qt::AlignVCenter );
}

QgsAppRibbonGroup *QgsAppRibbonPage::addGroup( const QString &title )
{
  auto *group = new QgsAppRibbonGroup( title, this );
  auto *separator = new QFrame( this );
  separator->setObjectName( u"HakeAppRibbonSeparator"_s );
  separator->setFrameShape( QFrame::NoFrame );
  separator->setFixedWidth( 1 );
  separator->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Expanding );

  // Keep the trailing stretch and overflow button at the end.
  const int insertAt = mLayout->count() - 2;
  mLayout->insertWidget( insertAt, group );
  mLayout->insertWidget( insertAt + 1, separator );
  mGroups << group;
  mSeparators << separator;
  return group;
}

void QgsAppRibbonPage::setMetrics( const QgsAppRibbonMetrics &metrics )
{
  // +1 bottom margin reserves the page's 1px bottom border drawn by the theme.
  mLayout->setContentsMargins( metrics.horizontalMargin, metrics.verticalMargin, metrics.horizontalMargin, metrics.verticalMargin + 1 );
  mOverflow->setFixedHeight( metrics.rowHeight );
  for ( QgsAppRibbonGroup *group : std::as_const( mGroups ) )
    group->setMetrics( metrics );
  relayout();
}

QSize QgsAppRibbonPage::sizeHint() const
{
  const QVector<QgsAppRibbonGroup::Level> levels( mGroups.size(), QgsAppRibbonGroup::Full );
  const QVector<bool> hidden( mGroups.size(), false );
  return QSize( requiredWidth( levels, hidden, false ), 0 );
}

QSize QgsAppRibbonPage::minimumSizeHint() const
{
  // Height is owned by QgsAppRibbon; width always adapts through relayout().
  return QSize( 0, 0 );
}

int QgsAppRibbonPage::requiredWidth( const QVector<QgsAppRibbonGroup::Level> &levels, const QVector<bool> &hidden, bool overflow ) const
{
  const QMargins margins = mLayout->contentsMargins();
  int width = margins.left() + margins.right();
  int items = 0;
  int visibleGroups = 0;
  for ( int i = 0; i < mGroups.size(); ++i )
  {
    if ( hidden[i] || !mGroups[i]->hasCommands() )
      continue;
    width += mGroups[i]->widthForLevel( levels[i] );
    ++visibleGroups;
    ++items;
  }
  if ( visibleGroups > 1 )
  {
    width += ( visibleGroups - 1 ) * mSeparators.first()->minimumWidth();
    items += visibleGroups - 1;
  }
  if ( overflow )
  {
    width += mOverflow->sizeHint().width();
    ++items;
  }
  if ( items > 1 )
    width += ( items - 1 ) * mLayout->spacing();
  return width;
}

void QgsAppRibbonPage::relayout()
{
  if ( mInRelayout || mGroups.isEmpty() )
    return;
  mInRelayout = true;

  const int available = width();
  const int n = static_cast<int>( mGroups.size() );
  QVector<QgsAppRibbonGroup::Level> levels( n, QgsAppRibbonGroup::Full );
  QVector<bool> hidden( n, false );
  bool overflow = false;

  auto fits = [&] { return requiredWidth( levels, hidden, overflow ) <= available; };

  // Compress one level at a time, rightmost group first, so the most-used
  // commands on the left keep their labels longest.
  bool done = fits();
  for ( int level = QgsAppRibbonGroup::Compact; !done && level <= QgsAppRibbonGroup::Collapsed; ++level )
  {
    for ( int i = n - 1; !done && i >= 0; --i )
    {
      // A collapsed drop-down carries a text label, so for small groups it can
      // be wider than the icon-only layout; only collapse when it saves space.
      const bool saves = level != QgsAppRibbonGroup::Collapsed
                         || mGroups[i]->widthForLevel( QgsAppRibbonGroup::Collapsed ) < mGroups[i]->widthForLevel( levels[i] );
      if ( levels[i] < level && saves )
      {
        levels[i] = static_cast<QgsAppRibbonGroup::Level>( level );
        done = fits();
      }
    }
  }
  if ( !done )
  {
    overflow = true;
    for ( int i = n - 1; i >= 0 && !fits(); --i )
      hidden[i] = true;
  }

  const QList<QMenu *> oldSubMenus = mOverflowMenu->findChildren<QMenu *>( Qt::FindDirectChildrenOnly );
  mOverflowMenu->clear();
  for ( QMenu *subMenu : oldSubMenus )
    subMenu->deleteLater();

  int lastVisible = -1;
  for ( int i = 0; i < n; ++i )
  {
    QgsAppRibbonGroup *group = mGroups[i];
    const bool hasCommands = group->hasCommands();
    group->setLevel( levels[i] );
    group->setVisible( hasCommands && !hidden[i] );
    if ( !hasCommands )
      continue;
    if ( hidden[i] )
    {
      QMenu *subMenu = mOverflowMenu->addMenu( group->title() );
      subMenu->addActions( group->commands() );
    }
    else
    {
      lastVisible = i;
    }
  }
  for ( int i = 0; i < n; ++i )
    mSeparators[i]->setVisible( mGroups[i]->isVisibleTo( this ) && i < lastVisible );
  mOverflow->setVisible( overflow );

  mInRelayout = false;
}

void QgsAppRibbonPage::resizeEvent( QResizeEvent *event )
{
  QWidget::resizeEvent( event );
  if ( event->size().width() != event->oldSize().width() )
    relayout();
}

void QgsAppRibbonPage::showEvent( QShowEvent *event )
{
  QWidget::showEvent( event );
  relayout();
}

//
// QgsAppRibbon
//

QgsAppRibbon::QgsAppRibbon( QWidget *parent, QgisApp *app )
  : QTabWidget( parent )
  , mApp( app )
{
  setObjectName( u"HakeAppRibbon"_s );
  setDocumentMode( true );
  setMovable( false );
  setUsesScrollButtons( true );
  // Height comes from measured metrics (sizeHint), never from a fixed pixel value.
  setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
  // Fusion leaves the empty region after the last tab unpainted unless QSS backgrounds are forced.
  // WA_StyledBackground is Qt-portable (Wayland-safe); do not use platform window APIs here.
  setAttribute( Qt::WA_StyledBackground, true );
  tabBar()->setAttribute( Qt::WA_StyledBackground, true );
  tabBar()->setAutoFillBackground( true );
  tabBar()->setExpanding( false );
  tabBar()->setDrawBase( false );
  tabBar()->setElideMode( Qt::ElideNone );
  tabBar()->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );

  // Brand at the right end of the tab strip; the tab bar is sized to end where it begins.
  auto *tabFiller = new QWidget( this );
  tabFiller->setObjectName( u"HakeAppRibbonTabFiller"_s );
  tabFiller->setAttribute( Qt::WA_StyledBackground, true );
  tabFiller->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
  auto *fillerLayout = new QHBoxLayout( tabFiller );
  fillerLayout->setContentsMargins( 0, 0, 0, 0 );
  mBrand = new QLabel( u"HAKE GEOSPATIAL"_s, tabFiller );
  mBrand->setObjectName( u"HakeAppRibbonBrand"_s );
  mBrand->setAccessibleName( tr( "Hake Geospatial" ) );
  fillerLayout->addWidget( mBrand, 0, Qt::AlignVCenter );
  setCornerWidget( tabFiller, Qt::TopRightCorner );

  if ( !mApp )
    return;

  // The ribbon tab strip is the only navigation row: exactly these nine tabs, in this order.
  // Classic menus (Project, Edit, View, Layer, Settings, Plugins, ...) are not tabs; they are
  // exposed as drop-downs inside the matching tab.

  // Home: project, editing, navigation and application settings
  {
    QgsAppRibbonPage *page = addPage( tr( "Home" ) );
    QgsAppRibbonGroup *project = addGroup( page, tr( "Project" ) );
    addNamedAction( project, u"mActionNewProject"_s, true );
    addNamedAction( project, u"mActionOpenProject"_s, true );
    addNamedAction( project, u"mActionSaveProject"_s, true );
    addNamedAction( project, u"mActionSaveProjectAs"_s );
    addNamedAction( project, u"mActionProjectProperties"_s );
    addNamedAction( project, u"mActionExit"_s );
    addMenu( project, mApp->projectMenu() );

    QgsAppRibbonGroup *edit = addGroup( page, tr( "Editing" ) );
    addNamedAction( edit, u"mActionUndo"_s );
    addNamedAction( edit, u"mActionRedo"_s );
    addMenu( edit, mApp->editMenu() );

    QgsAppRibbonGroup *navigation = addGroup( page, tr( "Navigation" ) );
    addNamedAction( navigation, u"mActionPan"_s, true );
    addNamedAction( navigation, u"mActionZoomIn"_s );
    addNamedAction( navigation, u"mActionZoomOut"_s );
    addNamedAction( navigation, u"mActionZoomFullExtent"_s );
    addNamedAction( navigation, u"mActionDraw"_s );

    QgsAppRibbonGroup *identify = addGroup( page, tr( "Identify" ) );
    addNamedAction( identify, u"mActionIdentify"_s, true );
    addNamedAction( identify, u"mActionOpenTable"_s );

    QgsAppRibbonGroup *application = addGroup( page, tr( "Application" ) );
    addNamedAction( application, u"mActionOptions"_s, true );
    addNamedAction( application, u"mActionStyleManager"_s );
    addNamedAction( application, u"mActionCustomProjection"_s );
    addMenu( application, mApp->settingsMenu() );
  }

  // Data: layer sources, layouts, database and web services
  {
    QgsAppRibbonPage *page = addPage( tr( "Data" ) );
    QgsAppRibbonGroup *layers = addGroup( page, tr( "Layers" ) );
    addNamedAction( layers, u"mActionDataSourceManager"_s, true );
    addNamedAction( layers, u"mActionAddOgrLayer"_s );
    addNamedAction( layers, u"mActionAddRasterLayer"_s );
    addNamedAction( layers, u"mActionAddMeshLayer"_s );
    addNamedAction( layers, u"mActionAddDelimitedText"_s );
    addNamedAction( layers, u"mActionAddSpatiaLiteLayer"_s );
    addNamedAction( layers, u"mActionAddVirtualLayer"_s );
    addNamedAction( layers, u"mActionAddWmsLayer"_s );
    addNamedAction( layers, u"mActionAddWfsLayer"_s );
    addNamedAction( layers, u"mActionLayerProperties"_s );
    addNamedAction( layers, u"mActionRemoveLayer"_s );
    addNamedAction( layers, u"mActionOpenTable"_s );
    addMenu( layers, mApp->layerMenu() );

    QgsAppRibbonGroup *layout = addGroup( page, tr( "Layout" ) );
    addNamedAction( layout, u"mActionNewPrintLayout"_s, true );
    addNamedAction( layout, u"mActionShowLayoutManager"_s );

    QgsAppRibbonGroup *services = addGroup( page, tr( "Services" ) );
    addMenu( services, mApp->databaseMenu() );
    addMenu( services, mApp->webMenu() );

    // Plugins (e.g. DB Manager, MetaSearch) populate these toolbars at runtime.
    mirrorToolbar( addGroup( page, tr( "Database" ) ), mApp->databaseToolBar() );
    mirrorToolbar( addGroup( page, tr( "Web" ) ), mApp->webToolBar() );
  }

  // Analysis
  {
    QgsAppRibbonPage *page = addPage( tr( "Analysis" ) );
    QgsAppRibbonGroup *measure = addGroup( page, tr( "Measure" ) );
    addNamedAction( measure, u"mActionMeasure"_s, true );
    addNamedAction( measure, u"mActionMeasureArea"_s );
    addNamedAction( measure, u"mActionMeasureBearing"_s );
    addNamedAction( measure, u"mActionMeasureAngle"_s );

    QgsAppRibbonGroup *statistics = addGroup( page, tr( "Statistics" ) );
    addNamedAction( statistics, u"mActionStatisticalSummary"_s );
    addNamedAction( statistics, u"mActionOpenFieldCalc"_s );

    QgsAppRibbonGroup *processing = addGroup( page, tr( "Processing" ) );
    addDockToggle( processing, u"ProcessingToolbox"_s, true );
    addNamedAction( processing, u"mActionShowPythonDialog"_s, true );
    addDeferredMenu( processing, u"processing"_s );
  }

  // Map: navigation, map views, bookmarks and panels
  {
    QgsAppRibbonPage *page = addPage( tr( "Map" ) );
    QgsAppRibbonGroup *navigation = addGroup( page, tr( "Navigation" ) );
    addNamedAction( navigation, u"mActionPan"_s, true );
    addNamedAction( navigation, u"mActionZoomIn"_s );
    addNamedAction( navigation, u"mActionZoomOut"_s );
    addNamedAction( navigation, u"mActionZoomFullExtent"_s );
    addNamedAction( navigation, u"mActionDraw"_s );
    addNamedAction( navigation, u"mActionPanToSelected"_s );
    addNamedAction( navigation, u"mActionZoomToSelected"_s );
    addNamedAction( navigation, u"mActionZoomToLayers"_s );
    addNamedAction( navigation, u"mActionZoomActualSize"_s );
    addNamedAction( navigation, u"mActionZoomLast"_s );
    addNamedAction( navigation, u"mActionZoomNext"_s );

    QgsAppRibbonGroup *mapViews = addGroup( page, tr( "Map Views" ) );
    addNamedAction( mapViews, u"mActionNewMapCanvas"_s, true );

    QgsAppRibbonGroup *views3d = addGroup( page, tr( "3D" ) );
    addNamedAction( views3d, u"mActionNew3DMapCanvas"_s, true );
    addNamedAction( views3d, u"mActionNew3DMapCanvasGlobe"_s, true );

    QgsAppRibbonGroup *bookmarks = addGroup( page, tr( "Bookmarks" ) );
    addNamedAction( bookmarks, u"mActionNewBookmark"_s, true );
    addNamedAction( bookmarks, u"mActionShowBookmarks"_s );

    QgsAppRibbonGroup *panels = addGroup( page, tr( "Panels" ) );
    addDockToggle( panels, u"Browser"_s );
    addDockToggle( panels, u"Layers"_s );
    addNamedAction( panels, u"mActionTemporalController"_s );
    addNamedAction( panels, u"mActionToggleFullScreen"_s );
    addMenu( panels, mApp->viewMenu() );
  }

  // Vector
  {
    QgsAppRibbonPage *page = addPage( tr( "Vector" ) );
    QgsAppRibbonGroup *digitize = addGroup( page, tr( "Digitizing" ) );
    addNamedAction( digitize, u"mActionToggleEditing"_s, true );
    addNamedAction( digitize, u"mActionSaveLayerEdits"_s );
    addNamedAction( digitize, u"mActionAddFeature"_s );
    addNamedAction( digitize, u"mActionVertexTool"_s );
    addNamedAction( digitize, u"mActionMoveFeature"_s );
    addNamedAction( digitize, u"mActionDeleteSelected"_s );
    addNamedAction( digitize, u"mActionCutFeatures"_s );
    addNamedAction( digitize, u"mActionCopyFeatures"_s );
    addNamedAction( digitize, u"mActionPasteFeatures"_s );

    QgsAppRibbonGroup *selection = addGroup( page, tr( "Selection" ) );
    addNamedAction( selection, u"mActionSelectFeatures"_s, true );
    addNamedAction( selection, u"mActionSelectPolygon"_s );
    addNamedAction( selection, u"mActionSelectByExpression"_s );
    addNamedAction( selection, u"mActionDeselectAll"_s );

    QgsAppRibbonGroup *labels = addGroup( page, tr( "Labels" ) );
    addNamedAction( labels, u"mActionLabeling"_s, true );
    addNamedAction( labels, u"mActionMoveLabel"_s );
    addNamedAction( labels, u"mActionRotateLabel"_s );
    addNamedAction( labels, u"mActionShowPinnedLabels"_s );
    addNamedAction( labels, u"mActionShowHideLabels"_s );

    QgsAppRibbonGroup *tools = addGroup( page, tr( "Tools" ) );
    addMenu( tools, mApp->vectorMenu() );
  }

  // Raster
  {
    QgsAppRibbonPage *page = addPage( tr( "Raster" ) );
    QgsAppRibbonGroup *stretch = addGroup( page, tr( "Stretch" ) );
    addNamedAction( stretch, u"mActionLocalHistogramStretch"_s, true );
    addNamedAction( stretch, u"mActionFullHistogramStretch"_s );
    addNamedAction( stretch, u"mActionLocalCumulativeCutStretch"_s );
    addNamedAction( stretch, u"mActionFullCumulativeCutStretch"_s );

    QgsAppRibbonGroup *brightness = addGroup( page, tr( "Brightness" ) );
    addNamedAction( brightness, u"mActionIncreaseBrightness"_s );
    addNamedAction( brightness, u"mActionDecreaseBrightness"_s );

    QgsAppRibbonGroup *contrast = addGroup( page, tr( "Contrast" ) );
    addNamedAction( contrast, u"mActionIncreaseContrast"_s );
    addNamedAction( contrast, u"mActionDecreaseContrast"_s );

    QgsAppRibbonGroup *gamma = addGroup( page, tr( "Gamma" ) );
    addNamedAction( gamma, u"mActionIncreaseGamma"_s );
    addNamedAction( gamma, u"mActionDecreaseGamma"_s );

    QgsAppRibbonGroup *tools = addGroup( page, tr( "Tools" ) );
    addNamedAction( tools, u"mActionShowRasterCalculator"_s );
    addMenu( tools, mApp->rasterMenu() );
  }

  // Extensions: extension management (existing plugin manager), plugin toolbar actions,
  // and top-level menus that installed extensions (e.g. HCMGIS) add at runtime.
  {
    QgsAppRibbonPage *page = addPage( tr( "Extensions" ) );
    QgsAppRibbonGroup *manage = addGroup( page, tr( "Manage" ) );
    addNamedAction( manage, u"mActionManagePlugins"_s, true );
    addNamedAction( manage, u"mActionShowPythonDialog"_s );
    addMenu( manage, mApp->pluginMenu() );

    mirrorToolbar( addGroup( page, tr( "Installed" ) ), mApp->pluginToolBar() );
    mExtensionMenus = addGroup( page, tr( "Installed Menus" ) );
  }

  // Mesh
  {
    QgsAppRibbonPage *page = addPage( tr( "Mesh" ) );
    QgsAppRibbonGroup *layers = addGroup( page, tr( "Layers" ) );
    addNamedAction( layers, u"mActionAddMeshLayer"_s, true );
    addNamedAction( layers, u"mActionNewMeshLayer"_s );

    mirrorToolbar( addGroup( page, tr( "Digitizing" ) ), mApp->meshToolBar() );

    QgsAppRibbonGroup *tools = addGroup( page, tr( "Tools" ) );
    addNamedAction( tools, u"mActionShowMeshCalculator"_s, true );
    addMenu( tools, mApp->meshMenu() );
  }

  // Help
  {
    QgsAppRibbonPage *page = addPage( tr( "Help" ) );
    QgsAppRibbonGroup *help = addGroup( page, tr( "Help" ) );
    addNamedAction( help, u"mActionHelpContents"_s, true );
    addMenu( help, mApp->helpMenu() );

    QgsAppRibbonGroup *documentation = addGroup( page, tr( "Documentation" ) );
    addNamedAction( documentation, u"mActionHelpAPI"_s );

    QgsAppRibbonGroup *about = addGroup( page, tr( "About" ) );
    addNamedAction( about, u"mActionAbout"_s, true );
    addNamedAction( about, u"mActionQgisHomePage"_s );
  }

  watchMenuBar( mApp->menuBar() );
  refreshOptionalActions();
  updateMetrics();
}

QSize QgsAppRibbon::sizeHint() const
{
  return QSize( tabBar()->sizeHint().width(), tabBar()->sizeHint().height() + mCommandHeight );
}

QSize QgsAppRibbon::minimumSizeHint() const
{
  return QSize( QTabWidget::minimumSizeHint().width(), tabBar()->sizeHint().height() + mCommandHeight );
}

void QgsAppRibbon::updateMetrics()
{
  if ( mUpdatingMetrics || mPages.isEmpty() )
    return;
  mUpdatingMetrics = true;

  ensurePolished();
  const QStyle *st = style();
  QgsAppRibbonMetrics m;
  m.smallIcon = st->pixelMetric( QStyle::PM_SmallIconSize, nullptr, this );
  m.largeIcon = st->pixelMetric( QStyle::PM_ToolBarIconSize, nullptr, this );
  const QFontMetrics fm = fontMetrics();
  m.verticalMargin = std::max( 2, fm.height() / 8 );
  m.horizontalMargin = fm.averageCharWidth();

  // Measure real, stylesheet-polished buttons (a page child picks up the
  // command-area QSS padding) instead of guessing sizes.
  {
    QToolButton probe( mPages.first() );
    QPixmap pixmap( m.largeIcon, m.largeIcon );
    pixmap.fill( Qt::transparent );
    probe.setIcon( QIcon( pixmap ) );
    probe.setText( u"Wg"_s );
    probe.setAutoRaise( true );
    probe.setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
    probe.setIconSize( QSize( m.smallIcon, m.smallIcon ) );
    probe.ensurePolished();
    m.rowHeight = probe.sizeHint().height();
    probe.setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
    probe.setIconSize( QSize( m.largeIcon, m.largeIcon ) );
    m.tallHeight = probe.sizeHint().height();
  }
  m.captionHeight = QFontMetrics( ribbonCaptionFont( font() ) ).height();

  if ( mBrand )
  {
    QFont brandFont = ribbonCaptionFont( font() );
    brandFont.setWeight( QFont::DemiBold );
    brandFont.setLetterSpacing( QFont::AbsoluteSpacing, 1.2 );
    mBrand->setFont( brandFont );
    mBrand->parentWidget()->layout()->setContentsMargins( m.horizontalMargin * 2, 0, m.horizontalMargin * 2, 0 );
  }

  // Page margins plus the 1px bottom border.
  const int chrome = 2 * m.verticalMargin + 1;
  const int command = chrome + m.tallHeight + m.captionHeight;
  m.buttonAreaHeight = m.tallHeight;
  m.rows = std::clamp( m.buttonAreaHeight / std::max( 1, m.rowHeight ), 1, 3 );
  m.tallPrimary = true;
  m.captions = true;

  if ( m != mMetrics || command != mCommandHeight )
  {
    mMetrics = m;
    mCommandHeight = command;
    for ( QgsAppRibbonPage *page : std::as_const( mPages ) )
      page->setMetrics( m );
  }
  updateGeometry();
  syncChromeTabBarGeometry();
  mUpdatingMetrics = false;
}

bool QgsAppRibbon::event( QEvent *event )
{
  const bool result = QTabWidget::event( event );
#if QT_VERSION >= QT_VERSION_CHECK( 6, 6, 0 )
  if ( event->type() == QEvent::DevicePixelRatioChange )
    updateMetrics();
#endif
  return result;
}

void QgsAppRibbon::changeEvent( QEvent *event )
{
  QTabWidget::changeEvent( event );
  if ( event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange )
    updateMetrics();
}

bool QgsAppRibbon::eventFilter( QObject *watched, QEvent *event )
{
  if ( ( event->type() == QEvent::ActionAdded || event->type() == QEvent::ActionRemoved ) && mMenuBar && watched == mMenuBar.data() )
  {
    // Extensions add or remove top-level menus in bursts; coalesce into one sync.
    if ( !mMenuBarSyncPending )
    {
      mMenuBarSyncPending = true;
      QTimer::singleShot( 0, this, [this] {
        mMenuBarSyncPending = false;
        syncMenuBar();
      } );
    }
  }
  else if ( event->type() == QEvent::ActionAdded || event->type() == QEvent::ActionRemoved )
  {
    auto *toolbar = qobject_cast<QToolBar *>( watched );
    if ( toolbar && mMirroredToolbars.contains( toolbar ) )
    {
      // Plugins often add several actions in a row; coalesce into one rebuild.
      if ( mPendingMirrorSyncs.isEmpty() )
      {
        QTimer::singleShot( 0, this, [this] {
          const QList<QPointer<QToolBar>> pending = std::exchange( mPendingMirrorSyncs, {} );
          for ( const QPointer<QToolBar> &pendingToolbar : pending )
          {
            if ( pendingToolbar )
              syncMirroredGroup( pendingToolbar );
          }
        } );
      }
      if ( !mPendingMirrorSyncs.contains( toolbar ) )
        mPendingMirrorSyncs << toolbar;
    }
  }
  return QTabWidget::eventFilter( watched, event );
}

void QgsAppRibbon::resizeEvent( QResizeEvent *event )
{
  QTabWidget::resizeEvent( event );
  syncChromeTabBarGeometry();
}

void QgsAppRibbon::showEvent( QShowEvent *event )
{
  QTabWidget::showEvent( event );
  syncChromeTabBarGeometry();
}

void QgsAppRibbon::syncChromeTabBarGeometry()
{
  QTabBar *bar = tabBar();
  if ( !bar )
    return;

  // Portable QWidget geometry only (valid on Wayland/X11/Windows/macOS). Document-mode
  // tab bars often keep sizeHint width (= tabs only); force the bar to span up to the
  // brand corner widget so the strip is filled past the last tab without stretching
  // tab labels, and the scroll arrows never sit underneath the brand.
  const int stripWidth = width();
  if ( stripWidth <= 0 )
    return;

  QWidget *filler = cornerWidget( Qt::TopRightCorner );
  const int barWidth = std::max( 1, stripWidth - ( filler ? filler->sizeHint().width() : 0 ) );

  if ( bar->minimumWidth() != barWidth )
    bar->setMinimumWidth( barWidth );

  // Integer height from sizeHint — avoid fighting layout when already the right width.
  const int h = std::max( bar->sizeHint().height(), 1 );
  if ( bar->x() != 0 || bar->width() != barWidth )
  {
    const QRect target( 0, bar->y(), barWidth, std::max( bar->height(), h ) );
    if ( bar->geometry() != target )
      bar->setGeometry( target );
  }

  if ( filler )
  {
    const int fillerH = std::max( bar->height(), h );
    if ( filler->minimumHeight() != fillerH )
      filler->setMinimumHeight( fillerH );
    if ( filler->maximumHeight() != fillerH )
      filler->setMaximumHeight( fillerH );
  }
}

void QgsAppRibbon::refreshOptionalActions()
{
  if ( !mApp )
    return;

  for ( QgsAppRibbonPage *page : std::as_const( mPages ) )
  {
    for ( QgsAppRibbonGroup *group : page->groups() )
    {
      if ( group->resolveDeferred( mApp ) )
        group->scheduleRebuild();
    }
  }
}

void QgsAppRibbon::refreshIcons()
{
  for ( QgsAppRibbonPage *page : std::as_const( mPages ) )
  {
    for ( QgsAppRibbonGroup *group : page->groups() )
      group->scheduleRebuild();
  }
}

QgsAppRibbonPage *QgsAppRibbon::addPage( const QString &title )
{
  auto *page = new QgsAppRibbonPage( this );
  addTab( page, title );
  mPages << page;
  return page;
}

QgsAppRibbonGroup *QgsAppRibbon::addGroup( QgsAppRibbonPage *page, const QString &title )
{
  return page->addGroup( title );
}

void QgsAppRibbon::addNamedAction( QgsAppRibbonGroup *group, const QString &objectName, bool primary )
{
  if ( !mApp || !group )
    return;
  if ( QAction *action = mApp->findChild<QAction *>( objectName ) )
    group->addEntry( { action, primary, QString() } );
}

void QgsAppRibbon::addDockToggle( QgsAppRibbonGroup *group, const QString &dockObjectName, bool primary )
{
  if ( group )
    group->addEntry( { nullptr, primary, dockObjectName } );
}

void QgsAppRibbon::addMenu( QgsAppRibbonGroup *group, QMenu *menu )
{
  if ( group && menu )
    group->addEntry( { menu->menuAction(), false, QString() } );
}

void QgsAppRibbon::addDeferredMenu( QgsAppRibbonGroup *group, const QString &menuObjectName )
{
  if ( group )
    group->addEntry( { nullptr, false, QString(), menuObjectName } );
}

void QgsAppRibbon::mirrorToolbar( QgsAppRibbonGroup *group, QToolBar *toolbar )
{
  if ( !group || !toolbar )
    return;
  mMirroredToolbars.insert( toolbar, group );
  toolbar->installEventFilter( this );
  const QList<QAction *> actions = toolbar->actions();
  for ( QAction *action : actions )
  {
    if ( QAction *presented = presentableToolbarAction( action ) )
      group->addEntry( { presented, false, QString() } );
  }
}

void QgsAppRibbon::syncMirroredGroup( QToolBar *toolbar )
{
  QgsAppRibbonGroup *group = mMirroredToolbars.value( toolbar );
  if ( !group )
    return;
  QList<QgsAppRibbonGroup::Entry> entries;
  const QList<QAction *> actions = toolbar->actions();
  for ( QAction *action : actions )
  {
    if ( QAction *presented = presentableToolbarAction( action ) )
      entries.append( QgsAppRibbonGroup::Entry { presented, false, QString() } );
  }
  group->setEntries( entries );
}

void QgsAppRibbon::watchMenuBar( QMenuBar *menuBar )
{
  if ( !menuBar )
    return;
  mMenuBar = menuBar;
  menuBar->installEventFilter( this );
  syncMenuBar();
}

bool QgsAppRibbon::isStandardMenu( const QMenu *menu ) const
{
  if ( !mApp || !menu )
    return false;
  // Processing is a core plugin with its own drop-down in the Analysis tab.
  if ( menu->objectName() == u"processing"_s )
    return true;
  const QList<const QMenu *> standardMenus {
    mApp->projectMenu(),
    mApp->editMenu(),
    mApp->viewMenu(),
    mApp->layerMenu(),
    mApp->settingsMenu(),
    mApp->pluginMenu(),
    mApp->vectorMenu(),
    mApp->rasterMenu(),
    mApp->databaseMenu(),
    mApp->webMenu(),
    mApp->meshMenu(),
    mApp->windowMenu(),
    mApp->helpMenu(),
  };
  return standardMenus.contains( menu );
}

void QgsAppRibbon::syncMenuBar()
{
  if ( !mApp || !mMenuBar )
    return;

  const QList<QAction *> barActions = mMenuBar->actions();
  const QList<QAction *> appActions = mApp->actions();
  for ( const QPointer<QAction> &adopted : std::as_const( mAdoptedMenuBarActions ) )
  {
    if ( adopted && !barActions.contains( adopted.data() ) )
      mApp->removeAction( adopted );
  }
  mAdoptedMenuBarActions.clear();

  QList<QgsAppRibbonGroup::Entry> extensionEntries;
  for ( QAction *action : barActions )
  {
    // The menu bar is hidden; registering its menus on the main window keeps their shortcuts active.
    if ( !appActions.contains( action ) )
      mApp->addAction( action );
    mAdoptedMenuBarActions << action;

    QMenu *menu = action->menu();
    if ( menu && !isStandardMenu( menu ) )
      extensionEntries.append( QgsAppRibbonGroup::Entry { action, false, QString() } );
  }

  if ( mExtensionMenus )
    mExtensionMenus->setEntries( extensionEntries );
}
