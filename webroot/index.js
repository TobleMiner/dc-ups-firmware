window.onerror = function(error, source, lineno, colno) {
    alert(error + source + lineno + colno);
};

window.onload = function() {
	var bindingManager = new BindingManager("ws://168.119.52.218:9655");
	var binding = bindingManager.bindReadonly(document.getElementsByClassName("js-wifi")[0], "wifi.status");
	var binding = bindingManager.bindReadonly(document.getElementsByClassName("js-ethernet")[0], "ethernet.status");
	var binding = bindingManager.bind(document.getElementsByClassName("js-input-number")[0], "stored.number");
	bindingManager.addEventListener("error", function(event) {
		console.log("Error: " + event);
	});
	bindingManager.connect();
};
