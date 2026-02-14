var osm = L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', { maxNativeZoom: 19, maxZoom: 23, attribution: '&copy; OpenStreetMap' });
var osmHOT = L.tileLayer('https://{s}.tile.openstreetmap.fr/hot/{z}/{x}/{y}.png', { maxNativeZoom: 19, maxZoom: 23, attribution: '&copy; OpenStreetMap HOT' });
var satellite = L.tileLayer('https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}', { maxNativeZoom: 17, maxZoom: 23, attribution: 'Tiles &copy; Esri' });
var darkMap = L.tileLayer('https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png', { attribution: '&copy; CartoDB', subdomains: 'abcd', maxNativeZoom: 19, maxZoom: 23 });
var lightMap = L.tileLayer('https://{s}.basemaps.cartocdn.com/light_all/{z}/{x}/{y}{r}.png', { attribution: '&copy; CartoDB', maxNativeZoom: 19, maxZoom: 23 });
var topoMap = L.tileLayer('https://{s}.tile.opentopomap.org/{z}/{x}/{y}.png', { maxNativeZoom: 17, maxZoom: 23, attribution: '&copy; OpenTopoMap'});

var baseMaps = {
    "Koyu (Dark)": darkMap,
    "Standart (OSM)": osm,
    "Insani (HOT)": osmHOT,
    "Uydu (Satellite)": satellite,
    "Aydınlık (Light)": lightMap,
    "Topografik": topoMap
};
var selectedLayer = baseMaps[startTheme] || darkMap;

var map = L.map('map', {
    attributionControl: false,
    zoomControl: false,
    center: [39.0, 35.0],
    zoom: 6,
    layers: [selectedLayer],
    maxZoom: 23
});

var missionLayer = L.layerGroup().addTo(map);
var zoneLayer = L.layerGroup().addTo(map);
var circleLayer = L.layerGroup().addTo(map);
var vehicleLayer = L.layerGroup().addTo(map);
var trailLayer = L.layerGroup().addTo(map);
var desiredLayer = L.layerGroup().addTo(map);

var trailPolyline = L.polyline([], {color: '#3b82f6', weight: 3}).addTo(trailLayer);

var overlayMaps = {
    "Gorev Yolu (WP)": missionLayer,
    "Bolgeler (Zones)": zoneLayer,
    "Cemberler (Circles)": circleLayer,
    "Arac (Vehicle)": vehicleLayer,
    "Ucus Izi": trailLayer,
    "Hedef (Desired)": desiredLayer
};

L.control.layers(baseMaps, overlayMaps, { position: 'topright' }).addTo(map);

map.on('baselayerchange', function(e) {
    window.location.hash = "theme:" + e.name;
});

var vehicleIcon = L.divIcon({
    className: '',
    html: '<div id="vehicle-arrow-inner" class="vehicle-container"><svg width="30" height="36" viewBox="0 0 30 36" style="display:block;"><path d="M15 2 L28 34 L2 34 Z" fill="#2563eb" stroke="white" stroke-width="3" stroke-linejoin="round" /></svg></div>',
    iconSize: [30, 36], iconAnchor: [15, 18]
});
var vehicleMarker = L.marker([0, 0], {icon: vehicleIcon, zIndexOffset: 1000});
var headingLine = L.polyline([], {color: '#fbbf24', weight: 3, dashArray: '5, 5'}).addTo(vehicleLayer);

var hasFirstFix = false;
var clickMode = "none"; 
var popup = L.popup({ maxWidth: 100 });
var isDraggingVehicle = false;
var lastVehicleMoveTime = 0;

function setMapCenter(lat, lon, zoom) { map.setView([lat, lon], zoom); }

function updateDrone(lat, lon, heading) {
    var newLatLng = new L.LatLng(lat, lon);
    
    if (!isDraggingVehicle) {
        vehicleMarker.setLatLng(newLatLng);
    }
    if (!vehicleLayer.hasLayer(vehicleMarker)) { vehicleMarker.addTo(vehicleLayer); }
    
    var arrow = document.getElementById('vehicle-arrow-inner');
    if(arrow) arrow.style.transform = 'rotate(' + heading + 'deg)';
    
    if(lat != 0 && lon != 0) {
        var d = 50; // 50m heading line
        var R = 6371000; 
        var brng = heading * Math.PI / 180;
        var lat1 = lat * Math.PI / 180;
        var lon1 = lon * Math.PI / 180;
        var lat2 = Math.asin(Math.sin(lat1)*Math.cos(d/R) + Math.cos(lat1)*Math.sin(d/R)*Math.cos(brng));
        var lon2 = lon1 + Math.atan2(Math.sin(brng)*Math.sin(d/R)*Math.cos(lat1), Math.cos(d/R)-Math.sin(lat1)*Math.sin(lat2));
        headingLine.setLatLngs([newLatLng, [lat2 * 180 / Math.PI, lon2 * 180 / Math.PI]]);
    }
    
    if (!hasFirstFix && lat != 0 && lon != 0) { map.panTo(newLatLng); hasFirstFix = true; }

    if(lat != 0 && lon != 0) {
        trailPolyline.addLatLng(newLatLng);
    }
}

function drawFence(points) {
    // Yasak bolge kaldirildi
}

function drawMission(points) {
    missionLayer.clearLayers();
    var polylineLatlngs = [];
    for (var i = 0; i < points.length; i++) {
        var loc = [points[i].lat, points[i].lon];
        if (i === 0) { 
            // Sadece gecerli bir konum varsa Home ikonunu ciz (0,0 degilse)
            if (points[i].lat !== 0 || points[i].lon !== 0) {
                L.marker(loc, {icon: L.divIcon({ className: 'home-icon', html: 'H', iconSize: [30, 30], iconAnchor: [15, 15] }), zIndexOffset: 500}).addTo(missionLayer); 
            }
        } 
        else { polylineLatlngs.push(loc); L.marker(loc, {icon: L.divIcon({ className: 'waypoint-icon', html: String(i), iconSize: [24, 24], iconAnchor: [12, 12] })}).addTo(missionLayer); }
    }
    if (polylineLatlngs.length > 1) { L.polyline(polylineLatlngs, {color: '#ef4444', weight: 4, dashArray: '10, 10'}).addTo(missionLayer); }
}

function drawMarkers(data) {
    zoneLayer.clearLayers(); circleLayer.clearLayers();
    
    if(data.zones) { 
        data.zones.forEach(function(zone) { 
            if(zone.length > 0) { 
                L.polygon(zone, {color: '#f97316', fillColor: '#f97316', fillOpacity: 0.2, dashArray: '5, 5'}).addTo(zoneLayer); 
                zone.forEach(function(pt) { 
                    L.circleMarker(pt, {color: '#f97316', radius: 3, fillOpacity: 1}).addTo(zoneLayer); 
                }); 
            } 
        }); 
    }
    
    if(data.currentZone && data.currentZone.length > 0) { data.currentZone.forEach(function(pt, idx) { L.circleMarker(pt, {color: '#f97316', radius: 4, fillOpacity: 1}).bindTooltip("zp"+(idx+1)).addTo(zoneLayer); }); if(data.currentZone.length > 1) { L.polyline(data.currentZone, {color: '#f97316', dashArray: '5,5'}).addTo(zoneLayer); } }
    if(data.circles) { data.circles.forEach(function(c) { L.circle([c.lat, c.lon], { color: '#22c55e', fillColor: '#22c55e', fillOpacity: 0.3, radius: c.r }).addTo(circleLayer); }); }
}

function drawDesiredWaypoint(lat, lon) {
    desiredLayer.clearLayers();
    if(lat != 0 && lon != 0) {
        var icon = L.divIcon({
            className: '',
            html: '<div style="width: 24px; height: 24px; border: 3px solid #d946ef; border-radius: 50%; background: rgba(217, 70, 239, 0.3); box-shadow: 0 0 10px #d946ef;"></div>',
            iconSize: [24, 24],
            iconAnchor: [12, 12]
        });
        L.marker([lat, lon], {icon: icon}).addTo(desiredLayer);
    }
}

function setClickMode(mode) { 
    clickMode = mode; 
    if(mode !== 'none') document.getElementById('map').style.cursor = 'crosshair'; 
    else document.getElementById('map').style.cursor = ''; 
    map.closePopup(); 
}

function setVehicleDraggable(enable) {
    if(vehicleMarker) {
        if(enable) {
            vehicleMarker.dragging.enable();
            vehicleMarker.on('dragstart', function(e) { isDraggingVehicle = true; });
            vehicleMarker.on('drag', function(e) {
                var now = Date.now();
                if (now - lastVehicleMoveTime > 50) {
                    var ll = e.target.getLatLng();
                    window.location.hash = "vehiclemoved:" + ll.lat + ":" + ll.lng;
                    lastVehicleMoveTime = now;
                }
            });
            vehicleMarker.on('dragend', function(e) {
                isDraggingVehicle = false;
                var ll = e.target.getLatLng();
                window.location.hash = "vehiclemoved:" + ll.lat + ":" + ll.lng;
            });
        } else {
            vehicleMarker.dragging.disable();
            vehicleMarker.off('dragstart');
            vehicleMarker.off('drag');
            vehicleMarker.off('dragend');
        }
    }
}

window.addWpFromPopup = function(lat, lng) { 
    window.location.hash = "addwp:" + lat + ":" + lng; 
    setTimeout(() => { window.location.hash = ""; }, 100);
    map.closePopup(); 
};
window.addZoneFromPopup = function(lat, lng) { 
    window.location.hash = "addzone:" + lat + ":" + lng; 
    setTimeout(() => { window.location.hash = ""; }, 100);
    map.closePopup(); 
};
window.copyCoord = function(lat, lng) { 
    window.location.hash = "copy:" + lat + ":" + lng; 
    setTimeout(() => { window.location.hash = ""; }, 100);
    map.closePopup(); 
};

map.on('click', function(e) {
    var lat = e.latlng.lat; var lng = e.latlng.lng;
    
    if(clickMode === 'waypoint') { 
        window.location.hash = "addwp:" + lat + ":" + lng; 
        setTimeout(() => { window.location.hash = ""; }, 100);
    } 
    else if(clickMode === 'zone') { 
        window.location.hash = "addzone:" + lat + ":" + lng; 
        setTimeout(() => { window.location.hash = ""; }, 100);
    }
    else {
        var coordTxt = lat.toFixed(6) + ", " + lng.toFixed(6);
        var html = '<div class="popup-coord" style="margin-bottom:3px; font-weight:bold; font-size:12px; text-align:center;">' + coordTxt + '</div>' +
                    '<div style="display:flex; flex-direction:column; gap:3px;">' +
                        '<button class="popup-btn" style="background-color:#565f89; color:white; border:none; padding:4px; border-radius:3px; cursor:pointer; font-size:11px;" onclick="copyCoord(' + lat + ',' + lng + ')">Kopyala</button>' +
                        '<button class="popup-btn" style="background-color:#3b82f6; color:white; border:none; padding:5px; border-radius:3px; cursor:pointer; font-size:11px;" onclick="addWpFromPopup(' + lat + ',' + lng + ')">Görev Ekle</button>' +
                        '<button class="popup-btn" style="background-color:#f97316; color:white; border:none; padding:4px; border-radius:3px; cursor:pointer; font-size:11px;" onclick="addZoneFromPopup(' + lat + ',' + lng + ')">Zone Ekle</button>' +
                    '</div>';
        
        popup.setLatLng(e.latlng).setContent(html).openOn(map);
    }
});

var CoordControl = L.Control.extend({
    options: { position: 'topleft' },
    onAdd: function (map) {
        this._map = map;
        this._div = L.DomUtil.create('div', 'leaflet-control leaflet-control-coord');
        this._div.style.background = 'rgba(26, 27, 38, 0.85)';
        this._div.style.color = '#7aa2f7';
        this._div.style.padding = '4px 8px';
        this._div.style.borderRadius = '4px';
        this._div.style.border = '1px solid #565f89';
        this._div.style.fontSize = '11px';
        this._div.style.fontFamily = 'Monospace';
        this._div.style.marginBottom = '0px';
        this._div.style.whiteSpace = 'nowrap';
        
        this._lat = 0; this._lng = 0; this._scale = "-";
        this.updateContent();
        
        map.on('zoomend moveend', this.updateScale, this);
        this.updateScale();
        
        return this._div;
    },
    onRemove: function(map) {
        map.off('zoomend moveend', this.updateScale, this);
    },
    updateScale: function() {
        var y = this._map.getSize().y / 2;
        var p1 = this._map.containerPointToLatLng([0, y]);
        var p2 = this._map.containerPointToLatLng([100, y]);
        var maxMeters = p1.distanceTo(p2);
        var meters = this._getRoundNum(maxMeters);
        this._scale = meters < 1000 ? meters + ' m' : (meters / 1000) + ' km';
        this.updateContent();
    },
    _getRoundNum: function (num) {
        var pow10 = Math.pow(10, (Math.floor(num) + '').length - 1), d = num / pow10;
        d = d >= 10 ? 10 : d >= 5 ? 5 : d >= 3 ? 3 : d >= 2 ? 2 : 1;
        return pow10 * d;
    },
    updateMouse: function(lat, lng) {
        this._lat = lat; this._lng = lng;
        this.updateContent();
    },
    updateContent: function() {
        this._div.innerHTML = "Lat: " + this._lat.toFixed(6) + ", Lon: " + this._lng.toFixed(6) + ", Scale: " + this._scale;
    }
});
var coordControl = new CoordControl();
coordControl.addTo(map);

L.control.zoom({ position: 'topleft' }).addTo(map);

map.on('mousemove', function(e) {
    coordControl.updateMouse(e.latlng.lat, e.latlng.lng);
});
