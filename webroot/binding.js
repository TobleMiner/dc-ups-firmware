'use strict';

var bindingPrototype = {
	_uiUpdate: function(newValue) {
		var request = new SetRequest(this, newValue);
		var requestId = this.manager.sendRequest(request);
		if (requestId === null) {
			
		} else {
			this.activeRequests[requestId] = newValue;
		}
	},

	_updateFailed: function(requestId) {
		delete this.activeRequests[requestId];
	},

	_updateSuccess: function(requestId) {
		this.value = this.activeRequests[requestId];
		delete this.activeRequests[requestId];
	},

	addEventListener: function(type, listener) {
		this.eventListeners[type].push(listener);
	},

	remoteUpdate: function(newValue) {
		this.updateFunc(this.elem, newValue);
		this.value = newValue;
	},

	update: function() {
		var getRequest = new GetRequest(this);
		this.manager.sendRequest(getRequest);
	}
};

function Binding(name, manager, elem, updateFunc) {
	this.name = name;
	this.manager = manager;
	this.eventListeners = {
		remoteUpdate: [],
		remoteUpdateError: []
	};
	this.activeRequests = { };
	this.elem = elem;
	var binding = this;
	elem.addEventListener("change", function(event) {
		binding._uiUpdate(event.target.value);
	});
	this.updateFunc = updateFunc;
}

Binding.prototype = bindingPrototype;

//Object.assign(Binding.prototype, bindingPrototype);

var setRequestPrototype = {
	getRequestString: function() {
		return "SET " + this.requestId.toString() + " " +
		       this.binding.name + " " + this.newValue;
	},

	handleResponse: function() {
		this.binding._updateSuccess(this.requestId);
	},

	handleTimeout: function() {
		this.binding._updateFailed(this.requestId);
	}
};

function SetRequest(binding, newValue) {
	this.timeout = null;
	this.requestId = null;
	this.binding = binding;
	this.newValue = newValue;
}

SetRequest.prototype = setRequestPrototype;
//Object.assign(SetRequest.prototype, setRequestPrototype);

var getRequestPrototype = {
	getRequestString: function() {
		return "GET " + this.requestId.toString() + " " +
		       this.binding.name;
	},

	handleResponse: function(payload) {
		this.binding.remoteUpdate(payload);
	},

	handleTimeout: function() {
		console.log("Get timeout");
	}
};

function GetRequest(binding) {
	this.timeout = null;
	this.requestId = null;
	this.binding = binding;
}

GetRequest.prototype = getRequestPrototype;
//Object.assign(GetRequest.prototype, getRequestPrototype);

var bindingManagerPrototype = {
	_handleRemoteUpdate: function(nameAndValue) {
		var nameEnd = nameAndValue.indexOf(" ");
		if (nameEnd === -1) {
			console.log("Invalid message from remote, no parameter name found");
			return;
		}
		var name = nameAndValue.substring(0, nameEnd);
		var value = nameAndValue.substring(nameEnd + 1);
		var binding = this.bindings[name];
		if (binding) {
			binding.remoteUpdate(value);
		} else {
			console.log("Got unsolicited update for unknown parameter " + name);
		}
	},

	_handleResponse: function(requestIdAndPayload) {
		var requestIdEnd = requestIdAndPayload.indexOf(" ");
		if (requestIdEnd === -1) {
			requestIdEnd = requestIdAndPayload.length;
		} else {
			requestIdEnd += 1;
		}
		var requestIdStr = requestIdAndPayload.substring(0, requestIdEnd);
		var requestId = parseInt(requestIdStr);
		var request = this.activeRequests[requestId];
		if (request) {
			var payload = requestIdAndPayload.substring(requestIdEnd);
			clearTimeout(request.timeout);
			request.handleResponse(payload);
			delete this.activeRequests[requestId];
		} else {
			console.log("Received response to unknown request (" + requestIdStr + "), expired?");
		}
	},

	_messageReceived: function(msg) {
		var verbEnd = msg.indexOf(" ");
		if (verbEnd === -1) {
			console.log("Invalid message from remote, no verb found");
			return;
		}
		var verb = msg.substring(0, verbEnd);
 		var remainder = msg.substring(verbEnd + 1);

		if (verb == "UPD") {
			this._handleRemoteUpdate(remainder);
		} else {
			this._handleResponse(remainder);
		}
	},

	_clearActiveRequests: function() {
		for (var key in this.activeRequests) {
			var request = this.activeRequests[key];
			request.handleTimeout();
			delete this.activeRequests[key];
		}
	},

	_updateBindings: function() {
		var bindings = this.bindings;
		for (var key in bindings) {
			bindings[key].update();
		}
	},

	sendRequest: function(request) {
		if (this.isConnected()) {
			var requestId = this.requestId++;
			request.requestId = requestId;
			this.activeRequests[requestId] = request;
			var manager = this;
			var timeout = setTimeout(function() {
				delete manager.activeRequests[requestId];
				request.handleTimeout();
			}, this.requestTimeoutMs);
			request.timeout = timeout;
			this.websocket.send(request.getRequestString());
			return requestId;
		}
		return null;
	},

	addEventListener: function(type, listener) {
		this.eventListeners[type].push(listener);
	},

	dispatchEvent: function(type, event) {
		var listeners = this.eventListeners[type];
		for (var i = 0; i < listeners.length; i++) {
			listeners[i](event);
		}
	},

	bind: function(elem, name) {
		var binding = new Binding(name, this, elem, function(elem, value) {
			elem.value = value;
		});
		if (this.isConnected()) {
			binding.update();
		}
		this.bindings[name] = binding;
		return binding;
	},

	bindReadonly: function(elem, name) {
		var binding = new Binding(name, this, elem, function(elem, value) {
			elem.innerText = value;
		});
		if (this.isConnected()) {
			binding.update();
		}
		this.bindings[name] = binding;
		return binding;
	},

	connect: function(url) {
		url = url || this.url;
		var manager = this;
		var websocket = new WebSocket(url);
		websocket.addEventListener("open", function() {
			manager.websocket = websocket;
			manager._updateBindings();
		});
		websocket.addEventListener("error", function(event) {
			manager.dispatchEvent("error", event);
		});
		websocket.addEventListener("message", function(event) {
			manager._messageReceived(event.data);
		});
		websocket.addEventListener("close", function(event) {
			manager.dispatchEvent("close", event);
			manager._clearActiveRequests();
		});
	},

	disconnect: function() {
		if (this.websocket) {
			this.websocket.close();
		}
		this._clearActiveRequests();
	},

	isConnected: function() {
		return this.websocket && this.websocket.readyState === 1;
	}
};

function BindingManager(url) {
	this.requestId = 0;
	this.activeRequests = { };
	this.websocket = null;
	this.requestTimeoutMs = 10000;
	this.url = url;
	this.eventListeners = {
		error: [],
		close: []
	};
	this.bindings = { };
}

BindingManager.prototype = bindingManagerPrototype;
//Object.assign(BindingManager.prototype, bindingManagerPrototype);
