import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import "components"

Item {
  id: welcomeScreen

  property color pageColor: "#EFF4F9"
  property color panelColor: "#FCFDFE"
  property color surfaceColor: "#F0F6FA"
  property color primaryColor: "#205C8D"
  property color accentColor: "#1687AD"
  property color accentSoftColor: "#DCEEF5"
  property color textColor: "#25384B"
  property color mutedTextColor: "#6B7E8F"
  property color borderColor: "#CAD9E5"
  property color statusColor: "#25875F"

  readonly property string uiFont: Qt.platform.os === "osx" ? ".AppleSystemUIFont" : Application.font.family
  readonly property bool narrowLayout: width < 960
  readonly property real layoutSizeFactor: width > 1200 && height > 800 ? 1.1 : 1.0

  Rectangle {
    anchors.fill: parent
    color: welcomeScreen.pageColor
    radius: 16
    border.width: 1
    border.color: welcomeScreen.borderColor
  }

  ColumnLayout {
    anchors.fill: parent
    anchors.margins: 24
    spacing: 16

    // Header
    RowLayout {
      Layout.fillWidth: true
      spacing: 14

      Image {
        Layout.preferredWidth: 56
        Layout.preferredHeight: 56
        source: "images/hake-gis-icon.png"
        fillMode: Image.PreserveAspectFit
        mipmap: true
        asynchronous: true
      }

      ColumnLayout {
        Layout.fillWidth: true
        spacing: 2

        Label {
          text: qsTr("HAKE GEOSPATIAL")
          color: welcomeScreen.accentColor
          font.family: welcomeScreen.uiFont
          font.pixelSize: 11
          font.weight: Font.Bold
        }
        Label {
          Layout.fillWidth: true
          text: productDisplayName
          color: welcomeScreen.textColor
          font.family: welcomeScreen.uiFont
          font.pixelSize: 22
          font.weight: Font.Bold
          elide: Text.ElideRight
        }
        Label {
          Layout.fillWidth: true
          text: qsTr("Welcome back  ·  Version %1").arg(appVersion)
          color: welcomeScreen.mutedTextColor
          font.family: welcomeScreen.uiFont
          font.pixelSize: 13
          font.weight: Font.Medium
          elide: Text.ElideRight
        }
      }

      RoundButton {
        id: closeButton
        Layout.preferredWidth: 36
        Layout.preferredHeight: 36
        Layout.alignment: Qt.AlignTop
        text: "×"
        hoverEnabled: true
        ToolTip.visible: hovered
        ToolTip.text: qsTr("Close")
        Accessible.name: qsTr("Close welcome")
        font.family: welcomeScreen.uiFont
        font.pixelSize: 22
        padding: 0
        onClicked: welcomeScreenController.hideScene()

        contentItem: Text {
          text: closeButton.text
          color: closeButton.hovered ? welcomeScreen.textColor : welcomeScreen.mutedTextColor
          font: closeButton.font
          horizontalAlignment: Text.AlignHCenter
          verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
          radius: width / 2
          color: closeButton.hovered ? welcomeScreen.accentSoftColor : welcomeScreen.panelColor
          border.width: 1
          border.color: welcomeScreen.borderColor
        }
      }
    }

    GridLayout {
      Layout.fillWidth: true
      Layout.fillHeight: true
      columns: welcomeScreen.narrowLayout ? 1 : 2
      columnSpacing: 20
      rowSpacing: 16

      // Left: hero, actions, recent projects
      ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredWidth: welcomeScreen.narrowLayout ? -1 : parent.width * 0.58
        spacing: 16

        Rectangle {
          Layout.fillWidth: true
          Layout.preferredHeight: heroColumn.implicitHeight + 40
          radius: 16
          color: welcomeScreen.panelColor
          border.width: 1
          border.color: welcomeScreen.borderColor

          ColumnLayout {
            id: heroColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 20
            spacing: 10

            Label {
              Layout.fillWidth: true
              text: qsTr("Welcome to Hake-GIS Desktop 2026")
              color: welcomeScreen.textColor
              font.family: welcomeScreen.uiFont
              font.pixelSize: 26
              font.weight: Font.Bold
              wrapMode: Text.WordWrap
            }
            Label {
              Layout.fillWidth: true
              text: qsTr("Turn spatial information into knowledge. Open a project, start something new, or catch up on the latest updates.")
              color: welcomeScreen.mutedTextColor
              font.family: welcomeScreen.uiFont
              font.pixelSize: 14
              lineHeight: 1.35
              wrapMode: Text.WordWrap
            }

            Flow {
              Layout.fillWidth: true
              Layout.topMargin: 6
              spacing: 10

              Button {
                id: gettingStartedButton
                implicitHeight: 40
                text: qsTr("Getting started")
                hoverEnabled: true
                Accessible.name: text
                onClicked: welcomeScreenController.openGettingStarted()
                contentItem: Text {
                  text: gettingStartedButton.text
                  color: welcomeScreen.accentColor
                  font.family: welcomeScreen.uiFont
                  font.pixelSize: 13
                  font.weight: Font.Bold
                  horizontalAlignment: Text.AlignHCenter
                  verticalAlignment: Text.AlignVCenter
                  leftPadding: 14
                  rightPadding: 14
                }
                background: Rectangle {
                  radius: 10
                  color: gettingStartedButton.hovered ? "#CFE6EF" : welcomeScreen.accentSoftColor
                }
              }

              Button {
                id: openProjectButton
                implicitHeight: 40
                text: qsTr("Open project")
                hoverEnabled: true
                Accessible.name: text
                onClicked: welcomeScreenController.openProjectDialog()
                contentItem: Text {
                  text: openProjectButton.text
                  color: "#F9FCFE"
                  font.family: welcomeScreen.uiFont
                  font.pixelSize: 13
                  font.weight: Font.Bold
                  horizontalAlignment: Text.AlignHCenter
                  verticalAlignment: Text.AlignVCenter
                  leftPadding: 14
                  rightPadding: 14
                }
                background: Rectangle {
                  radius: 10
                  color: openProjectButton.hovered ? "#174C77" : welcomeScreen.primaryColor
                }
              }

              Button {
                id: newProjectButton
                implicitHeight: 40
                text: qsTr("New project")
                hoverEnabled: true
                Accessible.name: text
                onClicked: {
                  welcomeScreenController.createBlankProject()
                  welcomeScreenController.hideScene()
                }
                contentItem: Text {
                  text: newProjectButton.text
                  color: welcomeScreen.textColor
                  font.family: welcomeScreen.uiFont
                  font.pixelSize: 13
                  font.weight: Font.Bold
                  horizontalAlignment: Text.AlignHCenter
                  verticalAlignment: Text.AlignVCenter
                  leftPadding: 14
                  rightPadding: 14
                }
                background: Rectangle {
                  radius: 10
                  color: newProjectButton.hovered ? welcomeScreen.accentSoftColor : welcomeScreen.surfaceColor
                  border.width: 1
                  border.color: welcomeScreen.borderColor
                }
              }

              Button {
                id: visitButton
                implicitHeight: 40
                text: qsTr("Visit Hake")
                hoverEnabled: true
                Accessible.name: text
                onClicked: Qt.openUrlExternally("https://haketech.com")
                contentItem: Text {
                  text: visitButton.text
                  color: welcomeScreen.textColor
                  font.family: welcomeScreen.uiFont
                  font.pixelSize: 13
                  font.weight: Font.Bold
                  horizontalAlignment: Text.AlignHCenter
                  verticalAlignment: Text.AlignVCenter
                  leftPadding: 14
                  rightPadding: 14
                }
                background: Rectangle {
                  radius: 10
                  color: visitButton.hovered ? welcomeScreen.accentSoftColor : welcomeScreen.surfaceColor
                  border.width: 1
                  border.color: welcomeScreen.borderColor
                }
              }
            }
          }
        }

        Rectangle {
          Layout.fillWidth: true
          Layout.fillHeight: true
          radius: 16
          color: welcomeScreen.panelColor
          border.width: 1
          border.color: welcomeScreen.borderColor
          clip: true

          ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 10

            RowLayout {
              Layout.fillWidth: true
              Label {
                Layout.fillWidth: true
                text: qsTr("Recent projects")
                color: welcomeScreen.textColor
                font.family: welcomeScreen.uiFont
                font.pixelSize: 16
                font.weight: Font.Bold
              }
              Label {
                visible: recentProjectsListView.count === 0
                text: qsTr("No recent projects yet")
                color: welcomeScreen.mutedTextColor
                font.family: welcomeScreen.uiFont
                font.pixelSize: 12
              }
            }

            ListView {
              id: recentProjectsListView
              Layout.fillWidth: true
              Layout.fillHeight: true
              spacing: 10
              clip: true
              model: recentProjectsModel

              delegate: ProjectCard {
                width: recentProjectsListView.width - 8
                layoutSizeFactor: welcomeScreen.layoutSizeFactor
                title: Title || ""
                subtitle: (ProjectNativePath || ProjectPath || "").replace(/([\\\/])/g, "$1\u200b")
                crs: Crs || ""
                imageSource: PreviewImagePath || ""
                isEnabled: Exists
                isPinned: Pinned
                isSelected: recentProjectsListView.currentIndex === index
                radius: 10

                onClicked: (mouse) => {
                  if (mouse.button == Qt.LeftButton && isEnabled) {
                    welcomeScreenController.openProject(ProjectPath)
                    welcomeScreenController.hideScene()
                  } else if (mouse.button == Qt.RightButton) {
                    recentProjectsMenu.projectIndex = index
                    recentProjectsMenu.projectPinned = Pinned
                    recentProjectsMenu.projectExists = Exists
                    recentProjectsMenu.projectHasNativePath = ProjectNativePath != ""
                    const point = mapToItem(recentProjectsListView, mouse.x, mouse.y)
                    recentProjectsMenu.popup(point.x, point.y)
                  }
                }
              }

              ScrollBar.vertical: CustomScrollBar {
                policy: ScrollBar.AsNeeded
              }

              Label {
                anchors.centerIn: parent
                visible: recentProjectsListView.count === 0
                text: qsTr("Open or create a project to see it here.")
                color: welcomeScreen.mutedTextColor
                font.family: welcomeScreen.uiFont
                font.pixelSize: 13
              }

              Menu {
                id: recentProjectsMenu
                property int projectIndex: 0
                property bool projectPinned: false
                property bool projectExists: false
                property bool projectHasNativePath: false

                background: Rectangle {
                  implicitWidth: 200
                  implicitHeight: 160
                  radius: 8
                  color: welcomeScreen.panelColor
                  border.color: welcomeScreen.borderColor
                  border.width: 1
                }

                MenuItem {
                  text: recentProjectsMenu.projectPinned ? qsTr("Unpin from List") : qsTr("Pin to List")
                  onClicked: {
                    if (recentProjectsMenu.projectPinned) {
                      recentProjectsModel.unpinProject(recentProjectsMenu.projectIndex)
                    } else {
                      recentProjectsModel.pinProject(recentProjectsMenu.projectIndex)
                    }
                  }
                }
                MenuItem {
                  text: qsTr("Refresh")
                  enabled: !recentProjectsMenu.projectExists
                  visible: enabled
                  height: enabled ? implicitHeight : 0
                  onClicked: recentProjectsModel.recheckProject(recentProjectsMenu.projectIndex)
                }
                MenuItem {
                  text: qsTr("Open Directory…")
                  enabled: recentProjectsMenu.projectExists && recentProjectsMenu.projectHasNativePath
                  visible: enabled
                  height: enabled ? implicitHeight : 0
                  onClicked: recentProjectsModel.openProject(recentProjectsMenu.projectIndex)
                }
                MenuItem {
                  text: qsTr("Remove from List")
                  onClicked: recentProjectsModel.removeProject(recentProjectsMenu.projectIndex)
                }
                MenuSeparator {}
                MenuItem {
                  text: qsTr("Clear List")
                  onClicked: welcomeScreenController.clearRecentProjects()
                }
              }
            }
          }
        }
      }

      // Right: news
      Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredWidth: welcomeScreen.narrowLayout ? -1 : parent.width * 0.42
        Layout.minimumHeight: welcomeScreen.narrowLayout ? 240 : -1
        radius: 16
        color: welcomeScreen.surfaceColor
        border.width: 1
        border.color: welcomeScreen.borderColor
        clip: true

        ColumnLayout {
          anchors.fill: parent
          anchors.margins: 20
          spacing: 12

          RowLayout {
            Layout.fillWidth: true
            Label {
              text: qsTr("RECENT NEWS")
              color: welcomeScreen.accentColor
              font.family: welcomeScreen.uiFont
              font.pixelSize: 11
              font.weight: Font.ExtraBold
            }
            Item { Layout.fillWidth: true }
            BusyIndicator {
              Layout.preferredWidth: 18
              Layout.preferredHeight: 18
              running: newsFeedParser.isFetching
              visible: running
            }
            Rectangle {
              Layout.preferredWidth: 8
              Layout.preferredHeight: 8
              radius: 4
              color: welcomeScreen.statusColor
              visible: newsFeedParser.enabled && newsListView.count > 0
            }
          }

          ListView {
            id: newsListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            clip: true
            visible: newsFeedParser.enabled && count > 0
            model: newsFeedModel

            delegate: Item {
              width: newsListView.width
              height: Math.max(96, newsColumn.implicitHeight + 20)

              RowLayout {
                anchors.fill: parent
                anchors.topMargin: 4
                anchors.bottomMargin: 12
                spacing: 12

                Rectangle {
                  Layout.preferredWidth: 32
                  Layout.preferredHeight: 32
                  Layout.alignment: Qt.AlignTop
                  radius: 12
                  color: welcomeScreen.accentSoftColor
                  Label {
                    anchors.centerIn: parent
                    text: "✦"
                    color: welcomeScreen.accentColor
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                  }
                }

                ColumnLayout {
                  id: newsColumn
                  Layout.fillWidth: true
                  spacing: 4

                  RowLayout {
                    Label {
                      text: qsTr("NEWS")
                      color: welcomeScreen.accentColor
                      font.family: welcomeScreen.uiFont
                      font.pixelSize: 10
                      font.weight: Font.Bold
                    }
                    Item { Layout.fillWidth: true }
                    RoundButton {
                      Layout.preferredWidth: 22
                      Layout.preferredHeight: 22
                      flat: true
                      text: "×"
                      Accessible.name: qsTr("Dismiss news")
                      onClicked: newsFeedParser.dismissEntry(Key)
                      contentItem: Text {
                        text: parent.text
                        color: welcomeScreen.mutedTextColor
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                      }
                      background: Rectangle {
                        radius: width / 2
                        color: parent.hovered ? welcomeScreen.accentSoftColor : "transparent"
                      }
                    }
                  }
                  Label {
                    Layout.fillWidth: true
                    text: Title || ""
                    color: welcomeScreen.textColor
                    font.family: welcomeScreen.uiFont
                    font.pixelSize: 14
                    font.weight: Font.Bold
                    wrapMode: Text.WordWrap
                  }
                  Label {
                    Layout.fillWidth: true
                    textFormat: Text.RichText
                    text: Content || ""
                    color: welcomeScreen.mutedTextColor
                    font.family: welcomeScreen.uiFont
                    font.pixelSize: 12
                    lineHeight: 1.35
                    wrapMode: Text.WordWrap
                    maximumLineCount: 4
                    elide: Text.ElideRight
                    onLinkActivated: link => Qt.openUrlExternally(link)
                  }
                  Label {
                    Layout.fillWidth: true
                    visible: Link != ""
                    text: qsTr("Read more")
                    color: welcomeScreen.accentColor
                    font.family: welcomeScreen.uiFont
                    font.pixelSize: 11
                    font.weight: Font.Bold
                    font.underline: readMoreArea.containsMouse
                    MouseArea {
                      id: readMoreArea
                      anchors.fill: parent
                      hoverEnabled: true
                      cursorShape: Qt.PointingHandCursor
                      onClicked: Qt.openUrlExternally(Link)
                    }
                  }
                }
              }

              Rectangle {
                visible: index < newsListView.count - 1
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: welcomeScreen.borderColor
              }
            }

            ScrollBar.vertical: CustomScrollBar {
              policy: ScrollBar.AsNeeded
            }
          }

          ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12
            visible: !newsListView.visible

            Label {
              Layout.fillWidth: true
              text: qsTr("What’s new")
              color: welcomeScreen.textColor
              font.family: welcomeScreen.uiFont
              font.pixelSize: 15
              font.weight: Font.Bold
            }
            Label {
              Layout.fillWidth: true
              text: qsTr("Stay updated on new features, releases, and product highlights from Hake Geospatial.")
              color: welcomeScreen.mutedTextColor
              font.family: welcomeScreen.uiFont
              font.pixelSize: 13
              lineHeight: 1.35
              wrapMode: Text.WordWrap
            }
            Item { Layout.fillHeight: true }
            Button {
              id: enableNewsButton
              Layout.fillWidth: true
              Layout.preferredHeight: 40
              visible: !newsFeedParser.enabled
              text: qsTr("Enable news feed")
              hoverEnabled: true
              onClicked: {
                newsFeedParser.enabled = true
                newsFeedParser.fetch()
              }
              contentItem: Text {
                text: enableNewsButton.text
                color: welcomeScreen.accentColor
                font.family: welcomeScreen.uiFont
                font.pixelSize: 13
                font.weight: Font.Bold
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
              }
              background: Rectangle {
                radius: 10
                color: enableNewsButton.hovered ? "#CFE6EF" : welcomeScreen.accentSoftColor
                border.width: 1
                border.color: welcomeScreen.borderColor
              }
            }
            Label {
              Layout.fillWidth: true
              visible: newsFeedParser.enabled && !newsFeedParser.isFetching && newsListView.count === 0
              text: qsTr("No news items right now. Check back later.")
              color: welcomeScreen.mutedTextColor
              font.family: welcomeScreen.uiFont
              font.pixelSize: 13
              wrapMode: Text.WordWrap
            }
          }
        }
      }
    }

    UpdateNotificationBar {
      id: pluginsUpdateBar
      Layout.fillWidth: true
      Layout.preferredHeight: 48
      radius: 12
      visible: false
      color: "#0f265c"
      onInstallClicked: {
        visible = false
        welcomeScreenController.showPluginManager()
      }
    }

    UpdateNotificationBar {
      id: qgisUpdateBar
      Layout.fillWidth: true
      Layout.preferredHeight: 48
      radius: 12
      visible: false
      color: "#0f265c"
      onInstallClicked: Qt.openUrlExternally("https://haketech.com")
    }
  }

  DropArea {
    anchors.fill: parent
    onDropped: drop => {
      let formatsData = {}
      for (const format of drop.formats) {
        formatsData[format] = drop.getDataAsArrayBuffer(format)
      }
      welcomeScreenController.forwardDrop(drop.text, drop.urls, formatsData)
    }
  }

  Connections {
    target: welcomeScreenController
    function onNewVersionAvailable(versionString) {
      qgisUpdateBar.message = qsTr("Hake Geospatial - Desktop %1 is out!").arg(versionString)
      qgisUpdateBar.visible = true
    }
    function onPluginUpdatesAvailable(plugins) {
      pluginsUpdateBar.message = qsTr("The following plugin(s) have available updates: %1", "", plugins.length).arg(plugins.join(", "))
      pluginsUpdateBar.visible = true
    }
  }

  Component.onCompleted: {
    if (newsFeedParser.enabled) {
      newsFeedParser.fetch()
    }
  }
}
