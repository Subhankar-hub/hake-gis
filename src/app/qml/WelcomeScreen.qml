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

  width: 400
  height: 600

  ColumnLayout {
    anchors.fill: parent
    anchors.margins: 8
    spacing: 8

    Item {
      Layout.fillWidth: true
      Layout.fillHeight: true

      // Soft shadow behind the card (avoids MultiEffect/layer issues in QQuickWidget)
      Rectangle {
        anchors.centerIn: welcomePanel
        width: welcomePanel.width
        height: welcomePanel.height
        radius: welcomePanel.radius
        color: "#280D2B45"
        opacity: 0.35
        z: 0
        transform: Translate { y: 8 }
      }

      Rectangle {
        id: welcomePanel
        width: Math.min(384, parent.width)
        height: Math.min(484, parent.height)
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        radius: 30
        color: welcomeScreen.panelColor
        border.width: 1
        border.color: welcomeScreen.borderColor
        z: 1
        clip: true

        ColumnLayout {
          anchors.fill: parent
          spacing: 0

          Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 108

            RowLayout {
              anchors.left: parent.left
              anchors.leftMargin: 24
              anchors.right: closeButton.left
              anchors.rightMargin: 12
              anchors.verticalCenter: parent.verticalCenter
              spacing: 13

              Image {
                id: logo
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
                  font.pixelSize: 10
                  font.weight: Font.Bold
                }
                Label {
                  Layout.fillWidth: true
                  text: productDisplayName
                  color: welcomeScreen.textColor
                  font.family: welcomeScreen.uiFont
                  font.pixelSize: 18
                  font.weight: Font.Bold
                  elide: Text.ElideRight
                }
                Label {
                  Layout.fillWidth: true
                  text: qsTr("Welcome back  ·  Version %1").arg(appVersion)
                  color: welcomeScreen.mutedTextColor
                  font.family: welcomeScreen.uiFont
                  font.pixelSize: 12
                  font.weight: Font.Medium
                  elide: Text.ElideRight
                }
              }
            }

            RoundButton {
              id: closeButton
              width: 32
              height: 32
              anchors.top: parent.top
              anchors.topMargin: 16
              anchors.right: parent.right
              anchors.rightMargin: 16
              text: "×"
              hoverEnabled: true
              ToolTip.visible: hovered
              ToolTip.text: qsTr("Close")
              Accessible.name: qsTr("Close welcome")
              font.family: welcomeScreen.uiFont
              font.pixelSize: 20
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

          Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 27
            color: welcomeScreen.surfaceColor

            ColumnLayout {
              anchors.fill: parent
              anchors.leftMargin: 24
              anchors.rightMargin: 24
              anchors.topMargin: 20
              anchors.bottomMargin: 20
              spacing: 0

              RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 20

                Label {
                  text: qsTr("RECENT NEWS")
                  color: welcomeScreen.accentColor
                  font.family: welcomeScreen.uiFont
                  font.pixelSize: 10
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
                  Rectangle {
                    anchors.centerIn: parent
                    width: 16
                    height: 16
                    radius: 8
                    color: "transparent"
                    border.width: 4
                    border.color: "#D9EFE5"
                    z: -1
                  }
                }
              }

              Item { Layout.preferredHeight: 12 }

              ListView {
                id: newsListView
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0
                clip: true
                visible: newsFeedParser.enabled && count > 0
                model: newsFeedModel

                delegate: Item {
                  id: newsDelegate

                  width: newsListView.width
                  height: Math.max(103, newsColumn.implicitHeight + 16)

                  RowLayout {
                    anchors.fill: parent
                    anchors.topMargin: 4
                    anchors.bottomMargin: 12
                    spacing: 12

                    Rectangle {
                      Layout.preferredWidth: 32
                      Layout.preferredHeight: 32
                      Layout.alignment: Qt.AlignTop
                      Layout.topMargin: 2
                      radius: 12
                      color: welcomeScreen.accentSoftColor

                      Label {
                        anchors.centerIn: parent
                        text: "✦"
                        color: welcomeScreen.accentColor
                        font.family: welcomeScreen.uiFont
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                      }
                    }

                    ColumnLayout {
                      id: newsColumn
                      Layout.fillWidth: true
                      Layout.alignment: Qt.AlignTop
                      spacing: 4

                      RowLayout {
                        spacing: 8
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
                        maximumLineCount: 3
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
                  text: qsTr("Welcome to %1").arg(productDisplayName)
                  color: welcomeScreen.textColor
                  font.family: welcomeScreen.uiFont
                  font.pixelSize: 15
                  font.weight: Font.Bold
                  wrapMode: Text.WordWrap
                }
                Label {
                  Layout.fillWidth: true
                  text: qsTr("Unlock the power of geo-located data with Hake Geospatial’s Desktop solutions. Stay updated on new features, releases, and community highlights.")
                  color: welcomeScreen.mutedTextColor
                  font.family: welcomeScreen.uiFont
                  font.pixelSize: 12
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
                    font.pixelSize: 12
                    font.weight: Font.Bold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                  }
                  background: Rectangle {
                    radius: 12
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
                  font.pixelSize: 12
                  wrapMode: Text.WordWrap
                }
              }
            }
          }

          Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 68

            RowLayout {
              anchors.fill: parent
              anchors.margins: 12
              spacing: 8

              Button {
                id: guideButton
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                hoverEnabled: true
                Accessible.name: qsTr("Getting started")
                onClicked: welcomeScreenController.openGettingStarted()

                contentItem: RowLayout {
                  spacing: 8
                  Label {
                    text: "▤"
                    color: welcomeScreen.accentColor
                    font.pixelSize: 15
                  }
                  Label {
                    text: qsTr("Getting started")
                    color: welcomeScreen.accentColor
                    font.family: welcomeScreen.uiFont
                    font.pixelSize: 12
                    font.weight: Font.Bold
                  }
                  Item { Layout.fillWidth: true }
                  Label {
                    text: "↗"
                    color: welcomeScreen.accentColor
                    font.pixelSize: 15
                  }
                }
                background: Rectangle {
                  radius: 12
                  color: guideButton.hovered ? "#CFE6EF" : welcomeScreen.accentSoftColor
                }
              }

              Button {
                id: visitButton
                Layout.preferredWidth: 104
                Layout.preferredHeight: 44
                text: qsTr("Visit Hake")
                hoverEnabled: true
                Accessible.name: text
                onClicked: Qt.openUrlExternally("https://haketech.com")
                contentItem: Text {
                  text: visitButton.text
                  color: "#F9FCFE"
                  font.family: welcomeScreen.uiFont
                  font.pixelSize: 12
                  font.weight: Font.Bold
                  horizontalAlignment: Text.AlignHCenter
                  verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                  radius: 10
                  color: visitButton.hovered ? "#174C77" : welcomeScreen.primaryColor
                }
              }
            }
          }
        }
      }
    }

    UpdateNotificationBar {
      id: pluginsUpdateBar
      Layout.fillWidth: true
      Layout.preferredHeight: 50
      radius: 16
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
      Layout.preferredHeight: 50
      radius: 16
      visible: false
      color: "#0f265c"

      onInstallClicked: {
        Qt.openUrlExternally("https://haketech.com")
      }
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
