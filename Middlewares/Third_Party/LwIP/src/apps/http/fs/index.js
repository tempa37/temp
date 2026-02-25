let ip_pattern = "^((1?[\\d]?[\\d]|2([0-4][\\d]|5[0-5]))[.]){3}(1?[\\d]?[\\d]|2([0-4][\\d]|5[0-5]))$";
let id_pattern = "^(2[0-4][0-7]|1[0-9][0-9]|[0-9][0-9]|[1-9]?)$"
let temp_pattern = "^[0-9]*[.]?[0-9]+$";

function LoadFile() {
	let upload = document.getElementById("fileinput");
	if (upload) {
		upload.addEventListener("change", function() {
			if (upload.files.length > 0) {
				let reader = new FileReader();
				reader.addEventListener('load', function() {
					// convert string to json object
					let settings_json = JSON.parse(reader.result);
					let file_status = 0;
					if ("usk_ip" in settings_json) {
						file_status++;
					}
					if ("usk_mask" in settings_json) {
						file_status++;
					}
					if ("usk_gateway" in settings_json) {
						file_status++;
					}
					if (file_status == 3) {
						window.sessionStorage.setItem("settings", JSON.stringify(settings_json))
						Set();
					} else {
						console.log("This not settings file");
					}
				});
				reader.readAsText(upload.files[0]);
			}
		});
	}
}

function SaveFile() {
	let data = window.sessionStorage.getItem("settings");
	let blob = new Blob([data], { type: "application/json" });
	let url = URL.createObjectURL(blob);
	let a = document.createElement("a");
	a.href = url;
	a.download = "settings.json";
	a.click();
	URL.revokeObjectURL(url);
}

function SendSettings() {
	document.getElementById("save").disabled = true;
	let re = new RegExp(ip_pattern);
	let validate_status = 0;
	let myObj = JSON.parse(window.sessionStorage.getItem("settings"));
	const grp = document.getElementById("usk");
	const elem = grp.getElementsByTagName("input");
	for (let i = 0; i < elem.length; i++) {
		const item = elem[i];
		if (item.name == "ip") {
			if (!re.test(item.value)) {
				validate_status++;
			} else {
				myObj[item.id] = item.value;
			}
		}
		if (item.name == "id") {
			if (item.value < 1 || item.value > 247) {
				validate_status++;
			} else {
				myObj[item.id] = item.value;
			}
		}
		if (item.name == "port") {
			if (item.value < 1 || item.value > 65536) {
				validate_status++;
			} else {
				myObj[item.id] = item.value;
			}
		}
	}
	let notify = document.getElementById("notice");
	document.getElementById("notice").style.color = "black";
	notify.textContent = "";
	let sensor_array = new Array();
	let relay_array = new Array();
	let eCount = 0;
	const gCount = Object.keys(myObj.group).length;
	for (let n = 0; n < gCount; n++) {
		eCount = Object.keys(myObj.group[n].temp).length;
		for (let i = 0; i < eCount; i++) {
			let item = document.getElementById(n+"temp"+i);
			let t = parseFloat(item.value);
			if (t < 0 || t > 1200) {
				validate_status++;
			} else {
				myObj.group[n].temp[i] = Math.round(t * 10);
			}
		}
		if (parseInt(myObj.group[n].temp[0]) >= parseInt(myObj.group[n].temp[1]) ||
		parseInt(myObj.group[n].temp[1]) >= parseInt(myObj.group[n].temp[2]) ||
		parseInt(myObj.group[n].temp[2]) >= parseInt(myObj.group[n].temp[3])) {
			validate_status++;
			notify.innerHTML += "Проверьте значения температурных порогов<br>";
		}
		if ("camera" in myObj.group[n]) {
			eCount = Object.keys(myObj.group[n].camera).length;
			for (let i = 0; i < eCount; i++) {
				const ip = document.getElementById("g"+n+myObj.group[n].camera[i].name+"ip");
				if (!re.test(ip.value)) {
					validate_status++;
				} else {
					myObj.group[n].camera[i].ip = ip.value;
				}
				const id = document.getElementById("g"+n+myObj.group[n].camera[i].name+"id");
				if (id.value < 1 || id.value > 247) {
					validate_status++;
				} else {
					myObj.group[n].camera[i].id = id.value;
				}
			}
		}
		if ("supply" in myObj.group[n]) {
			eCount = Object.keys(myObj.group[n].supply).length;
			for (let i = 0; i < eCount; i++) {
				const ip = document.getElementById("g"+n+myObj.group[n].supply[i].name+"ip");
				if (!re.test(ip.value)) {
					validate_status++;
				} else {
					myObj.group[n].supply[i].ip = ip.value;
				}
				const id = document.getElementById("g"+n+myObj.group[n].supply[i].name+"id");
				if (id.value < 1 || id.value > 247) {
					validate_status++;
				} else {
					myObj.group[n].supply[i].id = id.value;
				}
				const port = document.getElementById("g"+n+myObj.group[n].supply[i].name+"port");
				if (port.value < 1 || port.value > 65536) {
					validate_status++;
				} else {
					myObj.group[n].supply[i].port = port.value;
				}
			}
		}
		if ("relay" in myObj.group[n]) {
			eCount = Object.keys(myObj.group[n].relay).length;
			for (let i = 0; i < eCount; i++) {
				const id = document.getElementById("g"+n+myObj.group[n].relay[i].name+"id");
				if (id.value < 1 || id.value > 247) {
					validate_status++;
				} else {
					myObj.group[n].relay[i].id = id.value;
					if (relay_array.length == 0) {
						relay_array.push(id.value);
					} else {
						for (let relay_e of relay_array) {
							if (relay_e == id.value) {
								validate_status++;
								notify.innerHTML += "Повторное использование выхода "+relay_e+" реле.<br>";
							}
						}
						relay_array.push(id.value);
					}
				}
			}
		}
		if ("sensor" in myObj.group[n]) {
			eCount = Object.keys(myObj.group[n].sensor).length;
			for (let i = 0; i < eCount; i++) {
				const id = document.getElementById("g"+n+myObj.group[n].sensor[i].name+"id");
				if (id.value < 1 || id.value > 247) {
					validate_status++;
				} else {
					myObj.group[n].sensor[i].id = id.value;
					if (sensor_array.length == 0) {
						sensor_array.push(id.value);
					} else {
						for (let sensor_e of sensor_array) {
							if (sensor_e == id.value) {
								validate_status++;
								notify.innerHTML += "Повторное использование входа "+sensor_e+" датчика АСП.<br>";
							}
						}
						sensor_array.push(id.value);
					}
				}
			}
		}
	}
	if (validate_status > 0) {
		document.getElementById("save").disabled = false;
		document.getElementById("notice").style.color = "red";
		alert("При валидации данных произошла ошибка");
	} else {
		let data = JSON.stringify(myObj);
		let xhr = new XMLHttpRequest();
		xhr.open("POST", "/settings", true);
		xhr.setRequestHeader("Cache-Control", "no-cache, no-store, max-age=0");
		xhr.setRequestHeader("Content-Type","application/json; charset=utf-8");
		xhr.timeout = 10000;
		xhr.onload = function() {
			if (xhr.status != 200) {
				document.getElementById("notice").textContent = "Настройки не сохранены.";
				console.log("Settings data not transferred.");
				console.log(xhr.status + " : " + xhr.statusText);
				document.getElementById("save").disabled = false;
			} else {
				document.getElementById("notice").textContent = "Настройки сохранены.";
				setTimeout(function() { window.location.href = '/';}, 10000);
				console.log("Settings data transferred.");
			}
		};
		xhr.onerror = function() {
			console.log("Request failed");
		};
		xhr.send(data);
		delete xhr;
	}
}

function camera_dom(parent_tag, camera_obj, group_num) {
	if ("camera" in camera_obj) {
		let count = Object.keys(camera_obj.camera).length;
		for (let i = 0; i < count; i++) {
			const element_name = camera_obj.camera[i].name;
			let tag_0 = document.createElement("div");
			tag_0.className = "elem0";
			parent_tag.appendChild(tag_0);
			let tag_1 = document.createElement("div");
			tag_1.className = "left_col";
			tag_1.innerHTML = '<label for=g'+group_num+element_name+'ip>IP адрес IR камеры</label><span>&nbsp;['+element_name+']</span>';
			tag_0.appendChild(tag_1);
			tag_1 = document.createElement("div");
			tag_1.className = "right_col";
			tag_1.innerHTML = '<input placeholder="Укажите IP" required pattern='+ip_pattern+' type="text" name=camera id=g'+group_num+element_name+'ip>';
			tag_0.appendChild(tag_1);
			tag_0 = document.createElement("div");
			tag_0.className = "elem0";
			parent_tag.appendChild(tag_0);
			tag_1 = document.createElement("div");
			tag_1.className = "left_col";
			tag_1.innerHTML = '<label for=g'+group_num+element_name+'id>Modbus адрес IR камеры</label>';
			tag_0.appendChild(tag_1);
			tag_1 = document.createElement("div");
			tag_1.className = "right_col";
			tag_1.innerHTML = '<input placeholder="Укажите ID" required pattern='+id_pattern+' name=camera type="text" id=g'+group_num+element_name+'id>';
			tag_0.appendChild(tag_1);
			tag_1 = document.createElement("br");
			parent_tag.appendChild(tag_1);
		}
	}
}

function supply_dom(parent_tag, supply_obj, group_num) {
	if ("supply" in supply_obj) {
		let count = Object.keys(supply_obj.supply).length;
		for (let i = 0; i < count; i++) {
			const element_name = supply_obj.supply[i].name;
			let tag_0 = document.createElement("div");
			tag_0.className = "elem0";
			parent_tag.appendChild(tag_0);
			let tag_1 = document.createElement("div");
			tag_1.className = "left_col";
			tag_1.innerHTML = '<label for=g'+group_num+element_name+'ip>IP адрес источника питания</label><span>&nbsp;['+element_name+']</span>';
			tag_0.appendChild(tag_1);
			tag_1 = document.createElement("div");
			tag_1.className = "right_col";
			tag_1.innerHTML = '<input placeholder="Укажите IP" required pattern='+ip_pattern+' name=supply type=text id=g'+group_num+element_name+'ip>';
			tag_0.appendChild(tag_1);
			tag_0 = document.createElement("div");
			tag_0.className = "elem0";
			parent_tag.appendChild(tag_0);
			tag_1 = document.createElement("div");
			tag_1.className = "left_col";
			tag_1.innerHTML = '<label for=g'+group_num+element_name+'id>Modbus адрес источника питания</label>';
			tag_0.appendChild(tag_1);
			tag_1 = document.createElement("div");
			tag_1.className = "right_col";
			tag_1.innerHTML = '<input placeholder="Укажите ID" required required pattern='+id_pattern+' name=supply type=text id=g'+group_num+element_name+'id>';
			tag_0.appendChild(tag_1);
			tag_0 = document.createElement("div");
			tag_0.className = "elem0";
			parent_tag.appendChild(tag_0);
			tag_1 = document.createElement("div");
			tag_1.className = "left_col";
			tag_1.innerHTML = '<label for=g'+group_num+element_name+'port>TCP порт источника питания</label>';
			tag_0.appendChild(tag_1);
			tag_1 = document.createElement("div");
			tag_1.className = "right_col";
			tag_1.innerHTML = '<input placeholder="Укажите ID" required required pattern="\\d{1,5}" name=supply type=text id=g'+group_num+element_name+'port>';
			tag_0.appendChild(tag_1);
			tag_1 = document.createElement("br");
			parent_tag.appendChild(tag_1);
		}
	}
}

function relay_dom(parent_tag, relay_obj, group_num) {
	if ("relay" in relay_obj) {
		let count = Object.keys(relay_obj.relay).length;
		for (let i = 0; i < count; i++) {
			const element_name = relay_obj.relay[i].name;
			let tag_0 = document.createElement("div");
			tag_0.className = "elem0";
			parent_tag.appendChild(tag_0);
			let tag_1 = document.createElement("div");
			tag_1.className = "left_col";
			tag_1.innerHTML = '<label for=g'+group_num+element_name+'id>№ Выхода реле</label><span>&nbsp;['+element_name+']</span>';
			tag_0.appendChild(tag_1);
			tag_1 = document.createElement("div");
			tag_1.className = "right_col";
			tag_1.innerHTML = '<input placeholder="Укажите имя элемента" name=relay type=text id=g'+group_num+element_name+'id required pattern="[1-8]{1}">';
			tag_0.appendChild(tag_1);
			tag_1 = document.createElement("br");
			parent_tag.appendChild(tag_1);
		}
	}
}

function sensor_dom(parent_tag, sensor_obj, group_num) {
	if ("sensor" in sensor_obj) {
		let count = Object.keys(sensor_obj.sensor).length;
		for (let i = 0; i < count; i++) {
			const element_name = sensor_obj.sensor[i].name;
			let tag_0 = document.createElement("div");
			tag_0.className = "elem0";
			parent_tag.appendChild(tag_0);
			let tag_1 = document.createElement("div");
			tag_1.className = "left_col";
			tag_1.innerHTML = '<label for=g'+group_num+element_name+'id>№ входа АСП датчика</label><span>&nbsp;['+element_name+']</span>';
			tag_0.appendChild(tag_1);
			tag_1 = document.createElement("div");
			tag_1.className = "right_col";
			tag_1.innerHTML = '<input placeholder="Укажите имя элемента" name=sensor type=text id=g'+group_num+element_name+'id required pattern="^([0-1][0-0]|[1-9]?)$">';
			tag_0.appendChild(tag_1);
			tag_1 = document.createElement("br");
			parent_tag.appendChild(tag_1);
		}
	}
}

function temperature_dom(parent_tag, group_num) {
	for (let i = 0; i < 4; i++) {
		let tag_0 = document.createElement("div");
		tag_0.className = "elem0";
		parent_tag.appendChild(tag_0);
		let tag_1 = document.createElement("div");
		tag_1.className = "left_col";
		if (i == 0) {
			tag_1.innerHTML = '<label for='+group_num+'temp'+i+'>Порог отключения (сброс состояния), &deg;C</label>';
		} else {
			tag_1.innerHTML = '<label for='+group_num+'temp'+i+'>Порог включения (уровень '+i+'), &deg;C</label>';
		}
		tag_0.appendChild(tag_1);
		tag_1 = document.createElement("div");
		tag_1.className = "right_col";
		tag_1.innerHTML = '<input placeholder="Укажите температуру" required pattern='+temp_pattern+' type="text" name=temp id='+group_num+'temp'+i+'>';
		tag_0.appendChild(tag_1);
	}
	tag_1 = document.createElement("br");
	parent_tag.appendChild(tag_1);
}

async function update_dom() {
	let promise = new Promise((resolve, reject) => {
		let myObj = JSON.parse(window.sessionStorage.getItem("settings"));
		if (myObj.group != null) {
			let count = Object.keys(myObj.group).length;
			let br_tag = document.createElement("br");
			for (let group_num = 0; group_num < count; group_num++) {
				document.getElementById("button"+(group_num+1)).addEventListener("click", function() {
					const iframe = document.getElementById("group"+(group_num+1));
					const button = document.getElementById("button"+(group_num+1));
					iframe.classList.toggle("hide");
					button.innerHTML == "Спрятать" ? button.innerHTML = "Показать" : button.innerHTML = "Спрятать";
				});
				document.getElementById("frame"+(group_num+1)).style.display = "block";
				let parent_tag = document.querySelector("#group"+(group_num+1));
				parent_tag.className = "hide";
				parent_tag.appendChild(br_tag);
				temperature_dom(parent_tag, group_num);
				camera_dom(parent_tag, myObj.group[group_num], group_num);
				supply_dom(parent_tag, myObj.group[group_num], group_num);
				relay_dom(parent_tag, myObj.group[group_num], group_num);
				sensor_dom(parent_tag, myObj.group[group_num], group_num);
			}
			resolve();
		} else {
			reject(new Error("JSON NULL"));
		}
	});
}

function n(num, len) {
	return `${num}`.padStart(len, '0');
}

function camera_set(camera_obj, group_num) {
	if ("camera" in camera_obj) {
		let count = Object.keys(camera_obj.camera).length;
		for (let i = 0; i < count; i++) {
			let cam_name = camera_obj.camera[i].name
			let ip = document.getElementById("g"+group_num+cam_name+"ip");
			ip.value = camera_obj.camera[i].ip;
			let id = document.getElementById("g"+group_num+cam_name+"id");
			id.value = camera_obj.camera[i].id;
		}
	}
}

function supply_set(supply_obj, group_num) {
	if ("supply" in supply_obj) {
		let count = Object.keys(supply_obj.supply).length;
		for (let i = 0; i < count; i++) {
			let sup_name = supply_obj.supply[i].name;
			let ip = document.getElementById("g"+group_num+sup_name+"ip");
			ip.value = supply_obj.supply[i].ip;
			let id = document.getElementById("g"+group_num+sup_name+"id");
			id.value = supply_obj.supply[i].id;
			let port = document.getElementById("g"+group_num+sup_name+"port");
			port.value = supply_obj.supply[i].port;
		}
	}
}

function relay_set(relay_obj, group_num) {
	if ("relay" in relay_obj) {
		let count = Object.keys(relay_obj.relay).length;
		for (let i = 0; i < count; i++) {
			let relay_name = relay_obj.relay[i].name;
			let id = document.getElementById("g"+group_num+relay_name+"id");
			id.value = relay_obj.relay[i].id;
		}
	}
}

function sensor_set(sensor_obj, group_num) {
	if ("sensor" in sensor_obj) {
		let count = Object.keys(sensor_obj.sensor).length;
		for (let i = 0; i < count; i++) {
			let sensor_name = sensor_obj.sensor[i].name;
			let id = document.getElementById("g"+group_num+sensor_name+"id");
			id.value = sensor_obj.sensor[i].id;
		}
	}
}

function temperature_set(temperature_obj, group_num) {
	let count = Object.keys(temperature_obj.temp).length;
	for (let i = 0; i < count; i++) {
		let temp = document.getElementById(group_num+"temp"+i);
		temp.value = (temperature_obj.temp[i]/10).toFixed(1);
	}
}

async function Set() {
	let myObj = JSON.parse(window.sessionStorage.getItem("settings"));
	
	document.getElementById("ver").innerHTML = "Версия ПО "+myObj.ver;
	document.getElementById("ser").innerHTML = "Серийный № "+n(myObj.ser, 4);
	document.getElementById("mac").innerHTML = "MAC 00:10:A1:71:"+n(myObj.mac1, 2)+":"+n(myObj.mac0, 2);
	
	let min = myObj.uptm / 60;
	let hour = min / 60;
	let out_time = n(Math.floor(hour / 24), 3) +' д ' + n(Math.floor(hour % 24), 2) +' ч ' +
	n(Math.floor(min % 60), 2) +' м ' + n(Math.floor(myObj.uptm % 60), 2) + " c";
	document.getElementById("uptime").textContent = out_time;
	
	let el = document.getElementById("usk_ip");
	el.value = myObj.usk_ip;
	el.pattern = ip_pattern;
	el = document.getElementById("usk_mask");
	el.value = myObj.usk_mask;
	el.pattern = ip_pattern;
	el = document.getElementById("usk_gateway");
	el.value = myObj.usk_gateway;
	el.pattern = ip_pattern;
	el = document.getElementById("urm_id");
	el.value = myObj.urm_id;
	el.pattern = id_pattern;
	el = document.getElementById("umvh_id");
	el.value = myObj.umvh_id;
	el.pattern = id_pattern;
	document.getElementById("port_camera").value = myObj.port_camera;
	document.getElementById("rate").value = myObj.rate;
	document.getElementById("timeout").value = myObj.timeout;

	if (myObj.group != null) {
		let gCount = Object.keys(myObj.group).length;
		for (let n = 0; n < gCount; n++) {
			temperature_set(myObj.group[n], n);
			camera_set(myObj.group[n], n);
			supply_set(myObj.group[n], n);
			relay_set(myObj.group[n], n);
			sensor_set(myObj.group[n], n);
		}
	}
}

fetch('/stageset.json')
.then(function(response) { return response.json(); })
.then(function(json) {
	window.sessionStorage.setItem("settings", JSON.stringify(json));
	update_dom().then(Set());
})
.catch(error => {
    console.log(error);
});