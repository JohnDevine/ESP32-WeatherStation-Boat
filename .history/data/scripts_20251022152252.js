/**
 * ESP32 Configuration Portal JavaScript
 * Handles form interactions, AJAX requests, and dynamic UI updates
 * Author: John Devine <john.h.devine@gmail.com>
 */

// Global variables
let isLoading = false;

// Initialize when DOM is loaded
document.addEventListener('DOMContentLoaded', function() {
    initializeEventListeners();
    updateConnectionStatus();
    
    // Auto-load configuration on index page
    // Auto-load configuration on index or configuration pages
    if (window.location.pathname === '/' || window.location.pathname.includes('index.html')) {
        loadCurrentConfiguration();
    }

    if (window.location.pathname.includes('configuration.html')) {
        // Load config and update UI indicators
        loadCurrentConfiguration();
    }
});

/**
 * Initialize all event listeners
 */
function initializeEventListeners() {
    const configForm = document.getElementById('configForm');
    const loadConfigBtn = document.getElementById('loadConfigBtn');
    const saveConfigBtn = document.getElementById('saveConfigBtn');

    if (configForm) {
        configForm.addEventListener('submit', handleFormSubmit);
    }

    if (loadConfigBtn) {
        loadConfigBtn.addEventListener('click', loadCurrentConfiguration);
    }

    if (saveConfigBtn) {
        saveConfigBtn.addEventListener('click', handleFormSubmit);
    }

    // Add input validation
    addInputValidation();
}

/**
 * Add real-time input validation
 */
function addInputValidation() {
    const macInput = document.getElementById('macAddress');
    const ipInput = document.getElementById('ipAddress');
    const activeKeyInput = document.getElementById('activeKey');
    const pendingKeyInput = document.getElementById('pendingKey');

    if (macInput) {
        macInput.addEventListener('input', function() {
            validateMacAddress(this);
        });
    }

    if (ipInput) {
        ipInput.addEventListener('input', function() {
            validateIpAddress(this);
        });
    }

    if (activeKeyInput) {
        activeKeyInput.addEventListener('input', function() {
            validateHexKey(this);
        });
    }

    if (pendingKeyInput) {
        pendingKeyInput.addEventListener('input', function() {
            validateHexKey(this);
        });
    }
}

/**
 * Validate MAC address format
 */
function validateMacAddress(input) {
    const macPattern = /^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$/;
    const value = input.value.trim();
    
    if (value === '' || macPattern.test(value)) {
        input.setCustomValidity('');
        input.classList.remove('invalid');
    } else {
        input.setCustomValidity('Please enter a valid MAC address (e.g., AA:BB:CC:DD:EE:FF)');
        input.classList.add('invalid');
    }
}

/**
 * Validate IP address format
 */
function validateIpAddress(input) {
    const ipPattern = /^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/;
    const value = input.value.trim();
    
    if (value === '' || ipPattern.test(value)) {
        input.setCustomValidity('');
        input.classList.remove('invalid');
    } else {
        input.setCustomValidity('Please enter a valid IP address (e.g., 192.168.1.100)');
        input.classList.add('invalid');
    }
}

/**
 * Validate hexadecimal key format
 */
function validateHexKey(input) {
    const hexPattern = /^[0-9A-Fa-f]{32}$/;
    const value = input.value.trim();
    
    if (value === '' || hexPattern.test(value)) {
        input.setCustomValidity('');
        input.classList.remove('invalid');
    } else {
        input.setCustomValidity('Please enter exactly 32 hexadecimal characters');
        input.classList.add('invalid');
    }
}

/**
 * Handle form submission
 */
function handleFormSubmit(event) {
    event.preventDefault();
    
    if (isLoading) {
        return;
    }

    const form = document.getElementById('configForm');
    if (!form.checkValidity()) {
        form.reportValidity();
        return;
    }

    saveConfiguration();
}

/**
 * Load current configuration from ESP32
 */
function loadCurrentConfiguration() {
    if (isLoading) {
        return;
    }

    setLoading(true);
    showStatus('Loading configuration...', 'info');

    fetch('/get_config')
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            return response.json();
        })
        .then(data => {
            populateForm(data);
            updateCurrentConfigDisplay(data);
            showStatus('Configuration loaded successfully', 'success');
        })
        .catch(error => {
            console.error('Load configuration error:', error);
            showStatus('Failed to load configuration. Please check your connection.', 'error');
        })
        .finally(() => {
            setLoading(false);
        });
}

/**
 * Save configuration to ESP32
 */
function saveConfiguration() {
    if (isLoading) {
        return;
    }

    const formData = getFormData();
    
    setLoading(true);
    showStatus('Saving configuration...', 'info');

    fetch('/save_config', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify(formData)
    })
    .then(response => {
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }
        return response.json();
    })
    .then(data => {
        if (data.status === 'success') {
            showStatus(data.message || 'Configuration saved successfully!', 'success');
            // Reload the configuration to show saved values
            setTimeout(() => {
                loadCurrentConfiguration();
            }, 1000);
        } else {
            showStatus(data.message || 'Failed to save configuration', 'error');
        }
    })
    .catch(error => {
        console.error('Save configuration error:', error);
        showStatus('Failed to save configuration. Please check your connection.', 'error');
    })
    .finally(() => {
        setLoading(false);
    });
}

/**
 * Get form data as object
 */
function getFormData() {
    const macAddress = document.getElementById('macAddress')?.value.trim() || '';
    const ipAddress = document.getElementById('ipAddress')?.value.trim() || '';
    const password = document.getElementById('password')?.value.trim() || '';
    const activeKey = document.getElementById('activeKey')?.value.trim() || '';
    const pendingKey = document.getElementById('pendingKey')?.value.trim() || '';
    const bootCount = parseInt(document.getElementById('bootCount')?.value) || 0;
    const deviceRole = parseInt(document.getElementById('deviceRole')?.value) || 2;

    return {
        macAddress: macAddress,
        ipAddress: ipAddress,
        password: password,
        activeKey: activeKey,
        pendingKey: pendingKey,
        bootCount: bootCount,
        deviceRole: deviceRole
    };
}

/**
 * Populate form with data
 */
function populateForm(data) {
    const macAddress = document.getElementById('macAddress');
    const ipAddress = document.getElementById('ipAddress');
    const password = document.getElementById('password');
    const activeKey = document.getElementById('activeKey');
    const pendingKey = document.getElementById('pendingKey');
    const bootCount = document.getElementById('bootCount');
    const deviceRole = document.getElementById('deviceRole');

    if (macAddress && data.macAddress) {
        macAddress.value = data.macAddress;
    }
    
    if (ipAddress && data.ipAddress) {
        ipAddress.value = data.ipAddress;
    }
    
    if (password && data.password) {
        password.value = data.password;
    }
    
    if (activeKey && data.activeKey) {
        activeKey.value = data.activeKey;
    }
    
    if (pendingKey && data.pendingKey) {
        pendingKey.value = data.pendingKey;
    }
    
    if (bootCount && data.bootCount !== undefined) {
        bootCount.value = data.bootCount;
    }

    if (deviceRole && data.deviceRole !== undefined) {
        deviceRole.value = data.deviceRole;
    }
}

/**
 * Update current configuration display
 */
function updateCurrentConfigDisplay(data) {
    const configDataElement = document.getElementById('currentConfigData');
    if (!configDataElement) {
        return;
    }

    const html = `
        <div class="config-item">
            <span class="config-label">MAC Address:</span>
            <span class="config-value">${data.macAddress || 'Not set'}</span>
        </div>
        <div class="config-item">
            <span class="config-label">IP Address:</span>
            <span class="config-value">${data.ipAddress || 'Not set'}</span>
        </div>
        <div class="config-item">
            <span class="config-label">WiFi Password:</span>
            <span class="config-value">${data.password ? '***********' : 'Not set'}</span>
        </div>
        <div class="config-item">
            <span class="config-label">Active Key:</span>
            <span class="config-value">${data.activeKey || 'Not set'}</span>
        </div>
        <div class="config-item">
            <span class="config-label">Pending Key:</span>
            <span class="config-value">${data.pendingKey || 'Not set'}</span>
        </div>
        <div class="config-item">
            <span class="config-label">Device Role:</span>
            <span class="config-value">${data.deviceRole === 1 ? 'Gateway' : data.deviceRole === 2 ? 'Responder/Node' : 'Unknown'}</span>
        </div>
    `;

    configDataElement.innerHTML = html;
}

/**
 * Generate random encryption key
 */
function generateKey(inputId) {
    const input = document.getElementById(inputId);
    if (!input) {
        console.error(`Input element with ID '${inputId}' not found`);
        return;
    }

    // Generate 16 random bytes (32 hex characters)
    const randomKey = Array.from({ length: 16 }, () => {
        return Math.floor(Math.random() * 256).toString(16).padStart(2, '0').toUpperCase();
    }).join('');

    input.value = randomKey;
    
    // Trigger validation
    validateHexKey(input);
    
    showStatus(`Random key generated for ${inputId === 'activeKey' ? 'Active' : 'Pending'} Key`, 'info');
}

/**
 * Show status message
 */
function showStatus(message, type = 'info', duration = 5000) {
    const statusElement = document.getElementById('statusMessage');
    if (!statusElement) {
        console.log(`Status: [${type.toUpperCase()}] ${message}`);
        return;
    }

    statusElement.textContent = message;
    statusElement.className = `status-message ${type}`;
    statusElement.style.display = 'block';

    // Auto-hide after duration (except for errors)
    if (type !== 'error' && duration > 0) {
        setTimeout(() => {
            statusElement.style.display = 'none';
        }, duration);
    }

    // Scroll to status message if it's not visible
    statusElement.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
}

/**
 * Set loading state
 */
function setLoading(loading) {
    isLoading = loading;
    
    const saveBtn = document.getElementById('saveConfigBtn');
    const loadBtn = document.getElementById('loadConfigBtn');
    
    if (saveBtn) {
        saveBtn.disabled = loading;
        saveBtn.innerHTML = loading ? 
            '<span class="loading"></span> Saving...' : 
            '💾 Save Configuration';
    }
    
    if (loadBtn) {
        loadBtn.disabled = loading;
        loadBtn.innerHTML = loading ? 
            '<span class="loading"></span> Loading...' : 
            '📥 Load Current Config';
    }

    // Disable form inputs during loading
    const form = document.getElementById('configForm');
    if (form) {
        const inputs = form.querySelectorAll('input, button');
        inputs.forEach(input => {
            input.disabled = loading;
        });
    }
}

/**
 * Update connection status
 */
function updateConnectionStatus() {
    const statusElement = document.getElementById('connectionStatus');
    if (!statusElement) {
        return;
    }

    // Simple connectivity check
    fetch('/get_config', { method: 'HEAD' })
        .then(() => {
            statusElement.textContent = 'Connected';
            statusElement.style.color = 'var(--success-color)';
        })
        .catch(() => {
            statusElement.textContent = 'Disconnected';
            statusElement.style.color = 'var(--error-color)';
        });
}

/**
 * Format MAC address with colons
 */
function formatMacAddress(input) {
    let value = input.value.replace(/[^0-9A-Fa-f]/g, '');
    
    if (value.length > 12) {
        value = value.substring(0, 12);
    }
    
    const formatted = value.match(/.{1,2}/g)?.join(':') || value;
    input.value = formatted.toUpperCase();
}

/**
 * Copy text to clipboard
 */
function copyToClipboard(text) {
    if (navigator.clipboard && window.isSecureContext) {
        return navigator.clipboard.writeText(text).then(() => {
            showStatus('Copied to clipboard', 'success', 2000);
        }).catch(() => {
            showStatus('Failed to copy to clipboard', 'error', 3000);
        });
    } else {
        // Fallback for older browsers or non-HTTPS
        const textArea = document.createElement('textarea');
        textArea.value = text;
        textArea.style.position = 'fixed';
        textArea.style.left = '-999999px';
        textArea.style.top = '-999999px';
        document.body.appendChild(textArea);
        textArea.focus();
        textArea.select();
        
        try {
            document.execCommand('copy');
            textArea.remove();
            showStatus('Copied to clipboard', 'success', 2000);
        } catch (error) {
            textArea.remove();
            showStatus('Failed to copy to clipboard', 'error', 3000);
        }
    }
}

/**
 * Validate form before submission
 */
function validateForm() {
    const form = document.getElementById('configForm');
    if (!form) {
        return false;
    }

    const inputs = form.querySelectorAll('input[required]');
    let isValid = true;

    inputs.forEach(input => {
        if (!input.checkValidity()) {
            isValid = false;
            input.classList.add('invalid');
        } else {
            input.classList.remove('invalid');
        }
    });

    return isValid;
}

/**
 * Handle network errors gracefully
 */
function handleNetworkError(error) {
    console.error('Network error:', error);
    
    if (error.name === 'TypeError' && error.message.includes('fetch')) {
        showStatus('Connection lost. Please check your WiFi connection to the ESP32.', 'error');
    } else if (error.message.includes('timeout')) {
        showStatus('Request timed out. The ESP32 may be busy or disconnected.', 'error');
    } else {
        showStatus('Network error occurred. Please try again.', 'error');
    }
}

/**
 * Auto-retry failed requests
 */
function fetchWithRetry(url, options = {}, retries = 3) {
    return fetch(url, options).catch(error => {
        if (retries > 0) {
            console.log(`Retrying request to ${url}, ${retries} attempts left`);
            return new Promise(resolve => {
                setTimeout(() => resolve(fetchWithRetry(url, options, retries - 1)), 1000);
            });
        } else {
            throw error;
        }
    });
}

/**
 * Keyboard shortcuts
 */
document.addEventListener('keydown', function(event) {
    // Ctrl+S or Cmd+S to save
    if ((event.ctrlKey || event.metaKey) && event.key === 's') {
        event.preventDefault();
        if (!isLoading) {
            saveConfiguration();
        }
    }
    
    // Ctrl+L or Cmd+L to load
    if ((event.ctrlKey || event.metaKey) && event.key === 'l') {
        event.preventDefault();
        if (!isLoading) {
            loadCurrentConfiguration();
        }
    }
});

/**
 * Handle page visibility changes
 */
document.addEventListener('visibilitychange', function() {
    if (!document.hidden) {
        // Page became visible again, update connection status
        updateConnectionStatus();
    }
});

/**
 * Periodically update connection status
 */
setInterval(updateConnectionStatus, 30000); // Every 30 seconds

// Export functions for use in other scripts
window.ESP32Config = {
    generateKey: generateKey,
    showStatus: showStatus,
    copyToClipboard: copyToClipboard,
    loadCurrentConfiguration: loadCurrentConfiguration,
    saveConfiguration: saveConfiguration
};