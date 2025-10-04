let currentFile = null;
let retryCount = 0;
let maxRetries = 3;

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

function getDetailedErrorMessage(status) {
  switch (status) {
    case 400:
      return 'Firmware verification failed - This device requires properly signed firmware';
    case 413:
      return 'File too large (max 2MB)';
    case 500:
      return 'Server error during upload';
    case 0:
      return 'Network connection lost';
    default:
      return 'Upload failed (status: ' + status + ')';
  }
}

function showRetryButton() {
  document.getElementById('retryButton').style.display = 'inline-block';
}

function hideRetryButton() {
  document.getElementById('retryButton').style.display = 'none';
}

function retryUpload() {
  if (currentFile && retryCount < maxRetries) {
    retryCount++;
    uploadFirmware(null, true);
  }
}

function uploadFirmware(event, isRetry = false) {
  if (event) event.preventDefault();
  
  const fileInput = document.querySelector('input[type="file"]');
  const submitButton = document.querySelector('.btn-primary');
  const progressContainer = document.querySelector('.progress-container');
  const progressFill = document.querySelector('.progress-fill');
  const file = currentFile || fileInput.files[0];
  
  if (!file) {
    showStatus('Please select a firmware file', 'error');
    return;
  }
  
  if (!isRetry && !confirm('Are you sure you want to update the firmware?')) {
    return;
  }
  
  if (file.size > 2097152) {
    showStatus('File too large. Maximum size is 2MB', 'error');
    return;
  }
  
  submitButton.disabled = true;
  hideRetryButton();
  progressContainer.style.display = 'block';
  progressFill.style.width = '0%';
  
  showStatus(
    isRetry 
      ? '<span class="spinner"></span>Retrying upload... (Attempt ' + (retryCount + 1) + '/' + (maxRetries + 1) + ')'
      : '<span class="spinner"></span>Uploading firmware...',
    'uploading'
  );
  
  if (!isRetry) retryCount = 0;
  
  const xhr = new XMLHttpRequest();
  xhr.timeout = 60000;
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
    showStatus('Network error occurred', 'error');
    submitButton.disabled = false;
    progressContainer.style.display = 'none';
    if (retryCount < maxRetries) showRetryButton();
  };
  
  xhr.ontimeout = function() {
    showStatus('Upload timed out after 60 seconds', 'error');
    submitButton.disabled = false;
    progressContainer.style.display = 'none';
    if (retryCount < maxRetries) showRetryButton();
  };
  
  xhr.onreadystatechange = function() {
    if (xhr.readyState === 4) {
      progressContainer.style.display = 'none';
      if (xhr.status === 200) {
        showStatus('<span class="spinner"></span>Restarting system...', 'uploading');
        let countdown = 10;
        const restartInterval = setInterval(function() {
          showStatus(
            '<span class="spinner"></span>Restarting system... (' + countdown + ' seconds)',
            'uploading'
          );
          countdown--;
          if (countdown < 0) {
            clearInterval(restartInterval);
            document.title = 'Update Complete';
            showStatus(
              'Firmware uploaded successfully!<br><strong>System is restarting to apply the update.</strong><br>You can now close this window.',
              'success'
            );
            document.getElementById('uploadForm').style.display = 'none';
            hideRetryButton();
          }
        }, 1000);
      } else {
        const errorMsg = getDetailedErrorMessage(xhr.status);
        showStatus('Upload failed: ' + errorMsg, 'error');
        if (retryCount < maxRetries) {
          showRetryButton();
        } else {
          showStatus('Upload failed: ' + errorMsg + '<br>Maximum retry attempts reached.', 'error');
        }
      }
      submitButton.disabled = false;
    }
  };
  
  xhr.send(file);
}

window.onload = function() {
  setupDragDrop();
};
