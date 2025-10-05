let currentFile = null;

function setupDragDrop() {
  const dragDropArea = document.getElementById('dragDropArea');
  const fileInput = document.querySelector('input[type="file"]');
  
  dragDropArea.addEventListener('click', () => fileInput.click());
  
  dragDropArea.addEventListener('dragover', function(e) {
    e.preventDefault();
    dragDropArea.classList.add('dragover');
  });
  
  dragDropArea.addEventListener('dragleave', function(e) {
    e.preventDefault();
    dragDropArea.classList.remove('dragover');
  });
  
  dragDropArea.addEventListener('drop', function(e) {
    e.preventDefault();
    dragDropArea.classList.remove('dragover');
    const files = e.dataTransfer.files;
    if (files.length > 0) {
      const file = files[0];
      if (file.name.endsWith('.bin')) {
        fileInput.files = files;
        currentFile = file;
        updateFileDisplay(file);
      } else {
        showStatus('Please select a .bin file', 'error');
      }
    }
  });
  
  fileInput.addEventListener('change', function(e) {
    if (e.target.files.length > 0) {
      currentFile = e.target.files[0];
      updateFileDisplay(currentFile);
    }
  });
}

function updateFileDisplay(file) {
  const fileSize = (file.size / 1024 / 1024).toFixed(2);
  document.getElementById('dragDropArea').innerHTML = 
    '<div class="upload-icon">' +
    '<svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor">' +
    '<path stroke-linecap="round" stroke-linejoin="round" d="M9 12.75 11.25 15 15 9.75M21 12a9 9 0 1 1-18 0 9 9 0 0 1 18 0Z" />' +
    '</svg>' +
    '</div>' +
    '<p><strong>Selected:</strong> ' + file.name + '</p>' +
    '<p><strong>Size:</strong> ' + fileSize + ' MB</p>' +
    '<p>Click Upload Firmware to proceed</p>';
  document.getElementById('uploadButton').disabled = false;
}

function showStatus(msg, type = '') {
  const statusElem = document.getElementById('status');
  statusElem.innerHTML = msg;
  statusElem.className = 'status' + (type ? ' ' + type : '');
}

function getDetailedErrorMessage(status, responseText) {
  let serverMessage = '';
  try {
    const errorData = JSON.parse(responseText);
    if (errorData.error) {
      serverMessage = errorData.error;
    }
    if (errorData.details) {
      serverMessage += '<br><small>' + errorData.details + '</small>';
    }
  } catch (e) {
    if (responseText && responseText.length < 200) {
      serverMessage = responseText;
    }
  }
  
  switch (status) {
    case 400:
      return serverMessage || 'Invalid firmware file';
    case 413:
      const maxSizeMB = typeof CONFIG_MAX_FILE_SIZE_MB !== 'undefined' ? CONFIG_MAX_FILE_SIZE_MB : 2;
      return 'File too large (max ' + maxSizeMB + 'MB)';
    case 500:
      return serverMessage || 'Server error during upload';
    case 0:
      return 'Connection failed';

    case 503:
      return 'Device busy';
    default:
      return (serverMessage || 'Upload failed') + '<br><small>Error code: ' + status + '</small>';
  }
}



function validateFile(file) {
  const errors = [];
  
  // Check file existence
  if (!file) {
    errors.push('No file selected');
    return errors;
  }
  
  // Check file extension
  if (!file.name.toLowerCase().endsWith('.bin')) {
    errors.push('File must have .bin extension');
  }
  
  // Check file size (use configured limit)
  const maxSizeBytes = (typeof CONFIG_MAX_FILE_SIZE_MB !== 'undefined' ? CONFIG_MAX_FILE_SIZE_MB : 2) * 1024 * 1024;
  if (file.size > maxSizeBytes) {
    const maxSizeMB = Math.floor(maxSizeBytes / (1024 * 1024));
    errors.push('File too large (maximum ' + maxSizeMB + 'MB)');
  }
  
  // Check minimum size 
  if (file.size < 100 * 1024) {
    errors.push('File too small (minimum 100KB) - not valid firmware');
  }
  
  // Check file is not empty
  if (file.size === 0) {
    errors.push('File is empty');
  }
  
  return errors;
}

function uploadFirmware(event) {
  if (event) event.preventDefault();
  
  const fileInput = document.querySelector('input[type="file"]');
  const submitButton = document.querySelector('.btn-primary');
  const progressContainer = document.querySelector('.progress-container');
  const progressFill = document.querySelector('.progress-fill');
  const file = currentFile || fileInput.files[0];
  
  // Validate file before upload
  const validationErrors = validateFile(file);
  if (validationErrors.length > 0) {
    showStatus(
      '<strong>File Validation Failed</strong><br>' +
      validationErrors.join('<br>') +
      '<br><small>Please select a valid ESP32 firmware .bin file</small>',
      'error'
    );
    return;
  }
  
  if (!confirm(
    'Are you sure you want to update the firmware?\n\n' +
    'File: ' + file.name + '\n' +
    'Size: ' + (file.size / 1024 / 1024).toFixed(2) + ' MB\n\n' +
    'The device will restart after successful update.'
  )) {
    return;
  }
  
  submitButton.disabled = true;
  progressContainer.style.display = 'block';
  progressFill.style.width = '0%';
  
  showStatus('<span class="spinner"></span>Uploading firmware...');
  
  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/ota_update', true);
  xhr.setRequestHeader('Content-Type', 'application/octet-stream');
  
  xhr.upload.onprogress = function(event) {
    if (event.lengthComputable) {
      const percentComplete = (event.loaded / event.total) * 100;
      progressFill.style.width = percentComplete + '%';
      const speed = (event.loaded / 1024).toFixed(1);
      showStatus(
        '<span class="spinner"></span>Uploading... ' + 
        percentComplete.toFixed(1) + '% (' + speed + ' KB)',
        'uploading'
      );
    }
  };
  
  xhr.onerror = function() {
    showStatus(
      '<strong>Upload Failed</strong><br>' +
      'Network error occurred during upload',
      'error'
    );
    submitButton.disabled = false;
    progressContainer.style.display = 'none';
    console.error('XHR Network error during OTA upload');
  };
  
  xhr.onreadystatechange = function() {
    if (xhr.readyState === 4) {
      progressContainer.style.display = 'none';
      
      if (xhr.status === 200) {
        // Success - handle restart countdown
        showStatus('<span class="spinner"></span>Update successful! Restarting system...', 'success');
        let countdown = 15;
        const restartInterval = setInterval(function() {
          if (countdown > 0) {
            showStatus(
              '<span class="spinner"></span>Update successful! System restarting in ' + countdown + ' seconds...<br><small>Please wait for the device to restart</small>',
              'success'
            );
            countdown--;
          } else {
            clearInterval(restartInterval);
            document.title = 'Update Complete - System Restarted';
            showStatus(
              '<strong>Firmware update completed successfully!</strong><br>' +
              'The system has restarted with the new firmware.<br>' +
              '<small>You can now close this window or disconnect from the WiFi network.</small>',
              'success'
            );
            document.getElementById('uploadForm').style.display = 'none';
          }
        }, 1000);
        
      } else {
        // Error handling
        const errorMsg = getDetailedErrorMessage(xhr.status, xhr.responseText);
        showStatus('<strong>Upload Failed</strong><br>' + errorMsg, 'error');
        
        // Log error for debugging
        console.error('OTA Upload failed:', {
          status: xhr.status,
          statusText: xhr.statusText,
          responseText: xhr.responseText,
          file: file ? file.name : 'unknown'
        });
      }
      
      submitButton.disabled = false;
    }
  };
  
  xhr.send(file);
}

window.onload = function() {
  setupDragDrop();
};
