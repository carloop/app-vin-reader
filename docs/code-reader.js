var particle = new Particle();
var token;
var device;
var eventStream;
var $app = $("#app");

var codeUrl = "https://raw.githubusercontent.com/carloop/app-code-reader/master/src";
var appFiles = [
  "app-code-reader.cpp",
  "dtc.cpp",
  "dtc.h",
  "OBDMessage.cpp",
  "OBDMessage.h",
  "project.properties",
];

var templates = {
  loginForm: _.template($("#template-login-form").text()),
  selectDevice: _.template($("#template-select-device").text()),
  error: _.template($("#template-error").text()),
  mainUI: _.template($("#main-ui").text()),
};

function getToken() {
  token = localStorage.getItem('particle-token');
  return token;
}

function setToken(newToken) {
  token = newToken;
  localStorage.setItem('particle-token', token);
}

function getDevice() {
  device = localStorage.getItem('particle-device');
  return device;
}

function setDevice(newDevice) {
  device = newDevice;
  localStorage.setItem('particle-device', device);
}


function login() {
  if (!getToken()) {
    $app.html(templates.loginForm());
    $('#login-form').on('submit', function (event) {
      event.preventDefault();
      particleLogin()
    });
  } else {
    selectDeviceForm();
  }
}

function particleLogin() {
  var $username = $('#username');
  var $password = $('#password');
  var $submit = $('#login');
  var $errorMessage = $('#error-message');

  $username.prop('disabled', true);
  $password.prop('disabled', true);
  $submit.prop('disabled', true);

  var username = $username.val();
  var password = $password.val();

  particle.login({ username: username, password: password })
  .then(function (data) {
    setToken(data.body.access_token);
    selectDeviceForm();
  }, function (err) {
    var message = err.body && err.body.error_description || "User credentials are invalid";
    $errorMessage.html(message);

    $username.prop('disabled', false);
    $password.prop('disabled', false);
    $submit.prop('disabled', false);
  });
}

function selectDeviceForm(force) {
  if (force || !getDevice()) {
    particle.listDevices({ auth: token })
    .then(function (data) {
      var devices = data.body;
      $app.html(templates.selectDevice({ devices: devices}));
      $('[data-toggle="select"]').select2();
      
      $("#select-device").on("submit", function (event) {
        event.preventDefault();
        setDevice($("#device").val());
        mainUI();
      });
    })
    .catch(function (err) {
      showError();
    });
  } else {
    mainUI();
  }
}

function mainUI() {
  $app.html(templates.mainUI());

  var $flashButton = $("#flash-button");
  var $readButton = $("#read-codes");
  var $clearButton = $("#clear-codes");
  var $logoutButton = $("#logout-button");
  $flashButton.on('click', flashApp);
  $readButton.on('click', readCodes);
  $clearButton.on('click', clearCodes);
  $logoutButton.on('click', logout);
}

function timeoutPromise(ms) {
  return new Promise(function (fulfill, reject) {
    setTimeout(function () {
      reject(new Error("Timeout"));
    }, ms);
  });
}

function flashApp() {
  clearConsole();
  log("Start flashing...");
  var files = {};

  var filePromises = appFiles.map(function (f) {
    return $.ajax(codeUrl + "/" + f)
    .then(function (data) {
      files[f] = new Blob([data], { type: "text/plain" });
    });
  });

  Promise.all(filePromises)
  .then(function () {
    var flashPromise = particle.flashDevice({
      deviceId: device,
      files: files,
      auth: token
    });

    // Add timeout to flash
    return Promise.race([flashPromise, timeoutPromise(10000)]);
  })
  .then(function (data) {
    var body = data.body;

    if (body.ok) {
      setTimeout(function () {
        log("Done flashing!");
      }, 2000);
    } else {
      error("Error during flash.");
      error(body.errors.join("\n"));
    }
  }, function (err) {
    if (err.message == "Timeout") {
      error("Timeout during flash. Is your device connected to the Internet (breathing cyan)?");
      return;
    }

    throw err;
  })
  .catch(function (err) {
    console.error(err);
    showError();
  });
}

function readCodes() {
  setupEventStream()
  .then(function () {

    clearConsole();
    log("Reading codes...");

    var callPromise = particle.callFunction({
      deviceId: device,
      name: 'readCodes',
      argument: '',
      auth: token
    });

    // The result will be an event published by the device called codes/result

    // Add timeout to function call
    Promise.race([callPromise, timeoutPromise(10000)])
    .catch(function (err) {
      if (err.message == "Timeout") {
        error("Timeout. Is your device connected to the Internet (breathing cyan)?");
        return;
      }
      console.error(err);
      error("Error while reading codes. Try flashing the app to your Carloop.");
    });
  });
}

function clearCodes() {
  setupEventStream()
  .then(function () {

    clearConsole();
    log("Clearing codes...");

    var callPromise = particle.callFunction({
      deviceId: device,
      name: 'clearCodes',
      argument: '',
      auth: token
    });

    // The result will be an event published by the device called codes/cleared

    // Add timeout to function call
    Promise.race([callPromise, timeoutPromise(10000)])
    .catch(function (err) {
      if (err.message == "Timeout") {
        error("Timeout. Is your device connected to the Internet (breathing cyan)?");
        return;
      }
      console.error(err);
      error("Error while clearing codes. Try flashing the app to your Carloop.");
    });
  });
}

function log(message) {
  printToConsole(message, 'info');
}

function error(message) {
  printToConsole(message, 'error');
}

function printToConsole(message, type, rawHtml) {
  var $el = $('<div class="' + type + '"/>');
  if (rawHtml) {
    $el.html(message);
  } else {
    $el.text(message);
  }
  $("#console").append($el);
}

function clearConsole() {
  $("#console").html('');
}

function showError() {
  $app.html(templates.error());
}

function setupEventStream() {
  if (!eventStream) {
    return particle.getEventStream({
      deviceId: device,
      auth: token
    })
    .then(function (stream) {
      eventStream = stream;

      eventStream.on('codes/error', deviceError);
      eventStream.on('codes/result', deviceCodes);
      eventStream.on('codes/cleared', deviceCleared);
    });
  } else {
    return Promise.resolve();
  }
}

function deviceError(event) {
  error("Carloop is online but is it connected to a car with the ignition on?");
}

function deviceCodes(event) {
  try {
    var codes = parseCodes(event.data);
    displayCodes(codes);
  } catch (err) {
    console.error(err);
    error("Error while processing codes. Blame Julien for this bug");
  }
}

function parseCodes(encodedStr) {
  var types = {
    s: "stored",
    p: "pending",
    c: "cleared",
  };

  var codes = {};

  if (encodedStr === "null") {
    return codes;
  }

  encodedStr.split(",").forEach(function (encoded) {
    var type = types[encoded[encoded.length - 1]];
    var code = encoded.slice(0, encoded.length - 1);

    codes[type] = codes[type] || [];
    codes[type].push(code);
  });

  return codes;
}

function displayCodes(codes) {
  var types = Object.keys(codes);
  if (types.length == 0) {
    log("No codes. Awesome!");
  }
  
  types.forEach(function (type) {
    var codesOfType = codes[type];
    log(codesOfType.length + " " + type + " code" + (codesOfType.length > 1 ? "s" : "") + ":");
    codesOfType.forEach(function (code) {
      var html = '<a target="_blank" href="https://www.google.com/search?q=obdii+code+' + code + '">' + code + '</a> (click for more info)';
      printToConsole(html, 'info', true);
    });
  });
}

function deviceCleared(event) {
  log("Codes cleared!");
}

function logout() {
  setToken('');
  setDevice('');
  window.location.reload();
}


login();
