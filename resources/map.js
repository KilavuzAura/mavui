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
    center: [40.765, 29.940],
    zoom: 13,
    layers: [selectedLayer],
    maxZoom: 23
});

var missionLayer = L.layerGroup().addTo(map);
var zoneLayer = L.layerGroup().addTo(map);
var circleLayer = L.layerGroup().addTo(map);
var vehicleLayer = L.layerGroup().addTo(map);
var fenceLayer = L.layerGroup().addTo(map);
var trailLayer = L.layerGroup().addTo(map);

var trailPolyline = L.polyline([], {color: '#3b82f6', weight: 3}).addTo(trailLayer);

var overlayMaps = {
    "Gorev Yolu (WP)": missionLayer,
    "Bolgeler (Zones)": zoneLayer,
    "Cemberler (Circles)": circleLayer,
    "Arac (Vehicle)": vehicleLayer,
    "Yasak Bolgeler": fenceLayer,
    "Ucus Izi": trailLayer
};

L.control.layers(baseMaps, overlayMaps).addTo(map);

map.on('baselayerchange', function(e) {
    window.location.hash = "theme:" + e.name;
});

var vehicleIcon = L.divIcon({
    className: '',
    html: '<div id="vehicle-arrow-inner" class="vehicle-container"><svg width="30" height="36" viewBox="0 0 30 36" style="display:block;"><path d="M15 2 L28 34 L2 34 Z" fill="#2563eb" stroke="white" stroke-width="3" stroke-linejoin="round" /></svg></div>',
    iconSize: [30, 36], iconAnchor: [15, 18]
});
var vehicleMarker = L.marker([0, 0], {icon: vehicleIcon, zIndexOffset: 1000});

var hasFirstFix = false;
var clickMode = "none"; 
var popup = L.popup({ maxWidth: 100 });

function setMapCenter(lat, lon, zoom) { map.setView([lat, lon], zoom); }

function updateDrone(lat, lon, heading) {
    var newLatLng = new L.LatLng(lat, lon);
    
    vehicleMarker.setLatLng(newLatLng);
    if (!vehicleLayer.hasLayer(vehicleMarker)) { vehicleMarker.addTo(vehicleLayer); }
    
    var arrow = document.getElementById('vehicle-arrow-inner');
    if(arrow) arrow.style.transform = 'rotate(' + heading + 'deg)';
    
    if (!hasFirstFix && lat != 0 && lon != 0) { map.panTo(newLatLng); hasFirstFix = true; }

    if(lat != 0 && lon != 0) {
        trailPolyline.addLatLng(newLatLng);
    }
}

function drawFence(points) {
    fenceLayer.clearLayers();
    if(points && points.length > 0) {
        L.polygon(points, {
            color: '#ef4444',
            fillColor: '#ef4444',
            fillOpacity: 0.1,
            dashArray: '10, 5'
        }).addTo(fenceLayer);
    }
}

function drawMission(points) {
    missionLayer.clearLayers();
    var polylineLatlngs = [];
    for (var i = 0; i < points.length; i++) {
        var loc = [points[i].lat, points[i].lon];
        if (i === 0) { L.marker(loc, {icon: L.divIcon({ className: 'home-icon', html: 'H', iconSize: [30, 30], iconAnchor: [15, 15] }), zIndexOffset: 500}).addTo(missionLayer); } 
        else { polylineLatlngs.push(loc); L.marker(loc, {icon: L.divIcon({ className: 'waypoint-icon', html: String(i), iconSize: [24, 24], iconAnchor: [12, 12] })}).addTo(missionLayer); }
    }
    if (polylineLatlngs.length > 1) { L.polyline(polylineLatlngs, {color: '#ef4444', weight: 4, dashArray: '10, 10'}).addTo(missionLayer); }
}

function drawMarkers(data) {
    zoneLayer.clearLayers(); circleLayer.clearLayers();
    
    if(data.zones) { data.zones.forEach(function(zone) { if(zone.length > 0) { L.polygon(zone, {color: '#f97316', fillColor: '#f97316', fillOpacity: 0.3, weight: 2}).addTo(zoneLayer); zone.forEach(function(pt) { L.circleMarker(pt, {color: '#f97316', radius: 3, fillOpacity: 1}).addTo(zoneLayer); }); } }); }
    if(data.currentZone && data.currentZone.length > 0) { data.currentZone.forEach(function(pt, idx) { L.circleMarker(pt, {color: '#f97316', radius: 4, fillOpacity: 1}).bindTooltip("zp"+(idx+1)).addTo(zoneLayer); }); if(data.currentZone.length > 1) { L.polyline(data.currentZone, {color: '#f97316', dashArray: '5,5'}).addTo(zoneLayer); } }
    if(data.circles) { data.circles.forEach(function(c) { L.circle([c.lat, c.lon], { color: '#22c55e', fillColor: '#22c55e', fillOpacity: 0.3, radius: c.r }).addTo(circleLayer); }); }
}

function setClickMode(mode) { 
    clickMode = mode; 
    if(mode !== 'none') document.getElementById('map').style.cursor = 'crosshair'; 
    else document.getElementById('map').style.cursor = ''; 
    map.closePopup(); 
}

window.addWpFromPopup = function(lat, lng) { window.location.hash = "addwp:" + lat + ":" + lng; map.closePopup(); };
window.addZoneFromPopup = function(lat, lng) { window.location.hash = "addzone:" + lat + ":" + lng; map.closePopup(); };
window.copyCoord = function(lat, lng) { window.location.hash = "copy:" + lat + ":" + lng; map.closePopup(); };

map.on('click', function(e) {
    var lat = e.latlng.lat; var lng = e.latlng.lng;
    
    if(clickMode === 'waypoint') { 
        window.location.hash = "addwp:" + lat + ":" + lng; 
    } 
    else if(clickMode === 'zone') { 
        window.location.hash = "addzone:" + lat + ":" + lng; 
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

L.control.scale({imperial: false, metric: true}).addTo(map);