import QtQuick
import QtLocation
import QtPositioning

Item {
    id: root

    property string qsoMapTitle: "QSO map"
    property string qsoMapStatus: ""
    property var qsoMarkers: []
    property var qsoHome: ({ "valid": false, "grid": "", "lat": 0, "lon": 0 })
    property bool qsoShowPaths: false
    property bool qsoShowMaidenheadGrid: false
    property var qsoWorkedGridMap: ({})
    property bool triedNoKeyMapAfterApiError: false
    property var gridLines: buildGridLines()

    signal onlineMapUnavailable(string reason)

    function buildGridLines() {
        var lines = []
        for (var lon = -180; lon <= 180; lon += 20) {
            lines.push([QtPositioning.coordinate(-85, lon), QtPositioning.coordinate(85, lon)])
        }
        for (var lat = -80; lat <= 80; lat += 10) {
            lines.push([QtPositioning.coordinate(lat, -180), QtPositioning.coordinate(lat, 180)])
        }
        return lines
    }

    function selectNoKeyMapType() {
        if (!map.supportedMapTypes || map.supportedMapTypes.length <= 0) {
            return
        }

        // The Qt OSM plugin may expose provider-backed map types that require
        // API keys.  MM must work without an external key, so prefer the custom
        // OpenStreetMap tile host declared below.  Qt documents that this
        // CustomMap is appended as the last supportedMapTypes entry when
        // osm.mapping.custom.host is set.
        for (var i = map.supportedMapTypes.length - 1; i >= 0; --i) {
            if (map.supportedMapTypes[i].style === MapType.CustomMap) {
                map.activeMapType = map.supportedMapTypes[i]
                return
            }
        }

        // Fallback for older plugin builds: choose the plain OSM/street style,
        // avoiding provider names that commonly need API keys.
        for (var j = 0; j < map.supportedMapTypes.length; ++j) {
            var desc = (map.supportedMapTypes[j].description || "").toLowerCase()
            var name = (map.supportedMapTypes[j].name || "").toLowerCase()
            var text = desc + " " + name
            if ((text.indexOf("street") >= 0 || text.indexOf("openstreet") >= 0 || text.indexOf("osm") >= 0) &&
                text.indexOf("thunderforest") < 0 && text.indexOf("mapbox") < 0 &&
                text.indexOf("here") < 0 && text.indexOf("api key") < 0) {
                map.activeMapType = map.supportedMapTypes[j]
                return
            }
        }

        map.activeMapType = map.supportedMapTypes[0]
    }

    function resetView() {
        if (qsoHome && qsoHome.valid) {
            map.center = QtPositioning.coordinate(qsoHome.lat, qsoHome.lon)
            map.zoomLevel = 4.0
        } else {
            map.center = QtPositioning.coordinate(22.0, 10.0)
            map.zoomLevel = 2.0
        }
    }

    function retryOnlineMap() {
        triedNoKeyMapAfterApiError = false
        failTimer.restart()
        selectNoKeyMapType()
        resetView()
    }

    Plugin {
        id: osmPlugin
        name: "osm"
        // Use the public OpenStreetMap raster tiles directly and force the
        // corresponding CustomMap type below.  This avoids Qt's provider list
        // picking Thunderforest/Mapbox/HERE-like map types that require API keys.
        PluginParameter { name: "osm.useragent"; value: "MadModem/2.00 (QSO-map; GPL-3.0; contact=IZ6NNH)" }
        PluginParameter { name: "osm.mapping.providersrepository.disabled"; value: true }
        PluginParameter { name: "osm.mapping.host"; value: "https://tile.openstreetmap.org/" }
        PluginParameter { name: "osm.mapping.copyright"; value: "© OpenStreetMap contributors" }
        PluginParameter { name: "osm.mapping.custom.host"; value: "https://tile.openstreetmap.org/" }
        PluginParameter { name: "osm.mapping.custom.datacopyright"; value: "© OpenStreetMap contributors" }
        PluginParameter { name: "osm.mapping.custom.mapcopyright"; value: "© OpenStreetMap contributors" }
        PluginParameter { name: "osm.mapping.highdpi_tiles"; value: false }
    }

    Map {
        id: map
        anchors.fill: parent
        plugin: osmPlugin
        center: (qsoHome && qsoHome.valid) ? QtPositioning.coordinate(qsoHome.lat, qsoHome.lon)
                                          : QtPositioning.coordinate(22.0, 10.0)
        zoomLevel: (qsoHome && qsoHome.valid) ? 4.0 : 2.0
        minimumZoomLevel: 1.0
        maximumZoomLevel: 14.0
        gesture.enabled: true

        Component.onCompleted: {
            root.selectNoKeyMapType()
            failTimer.start()
        }
        onSupportedMapTypesChanged: root.selectNoKeyMapType()
        onCenterChanged: maidenheadCanvas.requestRepaint()
        onZoomLevelChanged: maidenheadCanvas.requestRepaint()
        onWidthChanged: maidenheadCanvas.requestRepaint()
        onHeightChanged: maidenheadCanvas.requestRepaint()
        onMapReadyChanged: {
            if (mapReady) {
                failTimer.stop()
            }
        }
        onErrorChanged: {
            if (error !== Map.NoError) {
                var msg = errorString && errorString.length > 0 ? errorString : ("Qt Location map error " + error)
                if (msg.toLowerCase().indexOf("api key") >= 0 && !root.triedNoKeyMapAfterApiError) {
                    root.triedNoKeyMapAfterApiError = true
                    root.selectNoKeyMapType()
                    return
                }
                onlineMapUnavailable(msg)
            }
        }

        Repeater {
            model: root.gridLines
            delegate: MapPolyline {
                line.width: 1
                line.color: "#708da6"
                path: modelData
            }
        }


        Canvas {
            id: maidenheadCanvas
            anchors.fill: parent
            visible: root.qsoShowMaidenheadGrid
            z: 8
            antialiasing: false

            function grid4FromLonLat(lon, lat) {
                if (lon < -180 || lon >= 180 || lat < -90 || lat >= 90)
                    return ""
                var x = lon + 180.0
                var y = lat + 90.0
                var lonField = Math.max(0, Math.min(17, Math.floor(x / 20.0)))
                var latField = Math.max(0, Math.min(17, Math.floor(y / 10.0)))
                var lonSquare = Math.max(0, Math.min(9, Math.floor((x - lonField * 20.0) / 2.0)))
                var latSquare = Math.max(0, Math.min(9, Math.floor(y - latField * 10.0)))
                return String.fromCharCode(65 + lonField) + String.fromCharCode(65 + latField) + lonSquare.toString() + latSquare.toString()
            }

            function requestRepaint() {
                if (visible)
                    requestPaint()
            }

            onVisibleChanged: requestRepaint()
            onWidthChanged: requestRepaint()
            onHeightChanged: requestRepaint()

            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                if (!root.qsoShowMaidenheadGrid || width <= 0 || height <= 0)
                    return

                var c1 = map.toCoordinate(Qt.point(0, 0), false)
                var c2 = map.toCoordinate(Qt.point(width, height), false)
                var minLon = Math.max(-180, Math.min(c1.longitude, c2.longitude))
                var maxLon = Math.min(180, Math.max(c1.longitude, c2.longitude))
                var minLat = Math.max(-90, Math.min(c1.latitude, c2.latitude))
                var maxLat = Math.min(90, Math.max(c1.latitude, c2.latitude))
                var firstLon = Math.max(-180, Math.floor(minLon / 2.0) * 2.0)
                var lastLon = Math.min(178, Math.ceil(maxLon / 2.0) * 2.0)
                var firstLat = Math.max(-90, Math.floor(minLat))
                var lastLat = Math.min(89, Math.ceil(maxLat))
                var worked = root.qsoWorkedGridMap || ({})

                ctx.font = "bold 11px sans-serif"
                ctx.textAlign = "center"
                ctx.textBaseline = "middle"

                for (var lon = firstLon; lon <= lastLon; lon += 2.0) {
                    for (var lat = firstLat; lat <= lastLat; lat += 1.0) {
                        var key = grid4FromLonLat(lon + 1.0, lat + 0.5)
                        if (key.length === 0)
                            continue
                        var p1 = map.fromCoordinate(QtPositioning.coordinate(lat + 1.0, lon), false)
                        var p2 = map.fromCoordinate(QtPositioning.coordinate(lat, lon + 2.0), false)
                        var x = Math.min(p1.x, p2.x)
                        var y = Math.min(p1.y, p2.y)
                        var w = Math.abs(p2.x - p1.x)
                        var h = Math.abs(p2.y - p1.y)
                        if (w < 1.0 || h < 1.0)
                            continue
                        var count = worked[key] || 0
                        if (count > 0) {
                            var a = Math.min(0.38, 0.18 + count * 0.045)
                            ctx.fillStyle = "rgba(0,150,70," + a + ")"
                            ctx.strokeStyle = "rgba(0,115,55,0.55)"
                            ctx.lineWidth = 1.2
                        } else {
                            ctx.fillStyle = "rgba(190,30,30,0.15)"
                            ctx.strokeStyle = "rgba(150,10,10,0.38)"
                            ctx.lineWidth = 0.8
                        }
                        ctx.fillRect(x, y, w, h)
                        ctx.strokeRect(x, y, w, h)

                        if (w >= 42 && h >= 20) {
                            ctx.fillStyle = count > 0 ? "rgba(0,65,28,0.88)" : "rgba(110,0,0,0.75)"
                            ctx.fillText(key, x + w / 2.0, y + h / 2.0)
                        }
                    }
                }
            }
        }

        Connections {
            target: root
            function onQsoShowMaidenheadGridChanged() { maidenheadCanvas.requestRepaint() }
            function onQsoWorkedGridMapChanged() { maidenheadCanvas.requestRepaint() }
        }

        MapItemView {
            model: (root.qsoShowPaths && root.qsoHome && root.qsoHome.valid) ? root.qsoMarkers : []
            delegate: MapPolyline {
                line.width: 2
                line.color: "#2d6fa8"
                path: [QtPositioning.coordinate(root.qsoHome.lat, root.qsoHome.lon),
                       QtPositioning.coordinate(modelData.lat, modelData.lon)]
            }
        }

        MapQuickItem {
            id: homeMarker
            visible: root.qsoHome && root.qsoHome.valid
            coordinate: visible ? QtPositioning.coordinate(root.qsoHome.lat, root.qsoHome.lon)
                                : QtPositioning.coordinate(0, 0)
            anchorPoint.x: 14
            anchorPoint.y: 14
            z: 50
            sourceItem: Item {
                width: 168
                height: 34
                Rectangle {
                    x: 0; y: 0; width: 28; height: 28; radius: 14
                    color: "white"; border.color: "#5a3000"; border.width: 2
                    Rectangle { x: 8; y: 12; width: 12; height: 10; color: "#ffd630"; border.color: "#5a3000" }
                    Canvas {
                        x: 5; y: 4; width: 18; height: 14
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            ctx.fillStyle = "#ffd630"
                            ctx.strokeStyle = "#5a3000"
                            ctx.lineWidth = 2
                            ctx.beginPath()
                            ctx.moveTo(1, 12)
                            ctx.lineTo(9, 2)
                            ctx.lineTo(17, 12)
                            ctx.closePath()
                            ctx.fill()
                            ctx.stroke()
                        }
                    }
                }
                Rectangle {
                    x: 34; y: 2; width: homeText.implicitWidth + 16; height: 24; radius: 5
                    color: "#f8fbff"; border.color: "#3d4a54"; border.width: 1
                    Text {
                        id: homeText
                        anchors.centerIn: parent
                        text: "HOME " + (root.qsoHome.grid || "")
                        color: "#182432"
                        font.bold: true
                        font.pixelSize: 14
                    }
                }
            }
        }

        MapItemView {
            model: root.qsoMarkers
            delegate: MapQuickItem {
                id: qsoMarker
                coordinate: QtPositioning.coordinate(modelData.lat, modelData.lon)
                anchorPoint.x: 6
                anchorPoint.y: 6
                z: 40
                sourceItem: Item {
                    id: markerRoot
                    property bool bubbleOpen: false
                    width: bubble.visible ? Math.max(132, bubbleText.implicitWidth + 20) : Math.max(80, callLabel.implicitWidth + 16)
                    height: bubble.visible ? bubble.height + 22 : 22
                    Rectangle {
                        id: dot
                        x: 0; y: 5; width: 12; height: 12; radius: 6
                        color: "#e64040"; border.color: "#600000"; border.width: 1
                    }
                    Rectangle {
                        id: labelBox
                        x: 14; y: 1; width: callLabel.implicitWidth + 10; height: 20; radius: 4
                        color: "#fff6e4"; border.color: "#6b5140"; border.width: 1
                        Text {
                            id: callLabel
                            anchors.centerIn: parent
                            text: modelData.callsign || "QSO"
                            color: "#1a1a1a"
                            font.pixelSize: 12
                            font.bold: true
                        }
                    }
                    Rectangle {
                        id: bubble
                        visible: markerRoot.bubbleOpen
                        x: 14; y: 24; width: Math.max(180, bubbleText.implicitWidth + 18); height: bubbleText.implicitHeight + 16
                        radius: 5
                        color: "#f8fbff"; border.color: "#304050"; border.width: 1
                        Text {
                            id: bubbleText
                            anchors.fill: parent
                            anchors.margins: 8
                            text: "<b>" + (modelData.callsign || "QSO") + "</b>\n" +
                                  "Mode: " + (modelData.mode || "") + "   Band: " + (modelData.band || "") + "\n" +
                                  "Grid: " + (modelData.grid || "") + "   UTC: " + (modelData.utc || "") + "\n" +
                                  ((modelData.distanceKm >= 0) ? ("Distance: " + Math.round(modelData.distanceKm) + " km   Bearing: " + Math.round(modelData.bearingDeg) + "°") : "")
                            textFormat: Text.RichText
                            color: "#102030"
                            font.pixelSize: 12
                        }
                    }
                    MouseArea {
                        id: markerMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton
                        onClicked: markerRoot.bubbleOpen = !markerRoot.bubbleOpen
                    }
                }
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 28
        color: "#d7e8f4"
        opacity: 0.92
        Text {
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            text: root.qsoMapTitle
            color: "#153247"
            font.pixelSize: 13
            font.bold: true
        }
        Text {
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            text: "Qt Location / OSM"
            color: "#37566a"
            font.pixelSize: 11
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 24
        color: "#d7e8f4"
        opacity: 0.92
        Text {
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            text: root.qsoMapStatus
            color: "#153247"
            font.pixelSize: 12
            font.bold: true
        }
    }

    Timer {
        id: failTimer
        interval: 12000
        repeat: false
        onTriggered: {
            if (!map.mapReady) {
                root.onlineMapUnavailable("Qt Location / OSM tiles unavailable; using offline world map")
            }
        }
    }
}
