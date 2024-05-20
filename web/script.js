const invidiousApiUrl = 'https://invidious.fdn.fr/api/v1';
let playlist = JSON.parse(localStorage.getItem('playlist')) || [];
let selectedVideos = [];
let currentPage = 1;
let searchQuery = '';
let currentlyPlaying = null;
let autoPlay = getBoolean('autoPlay') || false;
let progressSliderPos = 0;
let progressSliderMoved = false;
let volumeSliderPos = 0;
let volumeSliderMoved = false;
let apiUrlMPV = localStorage.getItem('apiUrlMPV') || 'http://localhost:8000';

document.addEventListener('DOMContentLoaded', function() {
	var toggleSettingsButton = document.getElementById('toggleSettings');
	var toggleDiv = document.getElementById('toggleDiv');

	toggleSettingsButton.addEventListener('click', function() {
		if (toggleDiv.style.display === "none" || toggleDiv.style.display === "") {
			toggleDiv.style.display = "flex";
		} else {
			toggleDiv.style.display = "none";
		}
		 adjustContentPadding();
	});
	   adjustContentPadding();

});

window.addEventListener('resize', adjustContentPadding);

// Adjust content padding based on header height
function adjustContentPadding() {
	var headerHeight = document.querySelector('.fixed-header').offsetHeight;
	document.querySelector('.container.my-5').style.paddingTop = headerHeight + 'px';
}



function saveBoolean(key, value) {
	localStorage.setItem(key, JSON.stringify(value));
}

function getBoolean(key) {
	const value = localStorage.getItem(key);
	return value !== null ? JSON.parse(value) : null;
}



// Function to set the global variable with input field content
function setApiUrl() {
	// Get the input field value
	apiUrlMPV = document.getElementById('apiUrlInput').value;
	localStorage.setItem('apiUrlMPV', apiUrlMPV);

}

// Add an event listener to the submit button
document.getElementById('apiSubmitButton').addEventListener('click', setApiUrl);


// Function to toggle dark mode
function toggleDarkMode() {
	document.body.classList.toggle('dark-mode');
	// Save the mode to localStorage
	if (document.body.classList.contains('dark-mode')) {
		localStorage.setItem('darkMode', 'enabled');
	} else {
		localStorage.setItem('darkMode', 'disabled');
	}
}

// Attach event listener to dark mode button
document.getElementById('darkModeButton').addEventListener('click', toggleDarkMode);

// Load the saved mode from localStorage
document.addEventListener('DOMContentLoaded', function() {
	const darkMode = localStorage.getItem('darkMode');
	if (darkMode === 'enabled') {
		document.body.classList.add('dark-mode');
	}
	const autoPlay = getBoolean('autoPlay');
	if (autoPlay) {
		toggleAutoPlay.classList.remove('btn-danger');
		toggleAutoPlay.classList.add('btn-success');
		toggleAutoPlay.textContent = "Auto Play";
	} else {
		toggleAutoPlay.classList.remove('btn-success');
		toggleAutoPlay.classList.add('btn-danger');
		toggleAutoPlay.textContent = "Auto Play";
	}

});


// Function to fetch video details from Invidious API
async function fetchVideoDetails(videoId) {
	const response = await fetch(`${invidiousApiUrl}/videos/${videoId}?region=FR`);
	if (response.ok) {
		const data = await response.json();
		return data;
	} else {
		return null;
	}
}

// Function to handle adding video by URL
async function addVideoByUrl() {
	const urlInput = document.getElementById('urlInput');
	const url = urlInput.value.trim();
	const videoId = extractVideoIdFromUrl(url);

	if (videoId) {
		const videoDetails = await fetchVideoDetails(videoId);
		if (videoDetails) {
			const video = {
				videoId: videoDetails.videoId,
				title: videoDetails.title,
				videoThumbnails: [{
					url: videoDetails.videoThumbnails[0].url
				}]
			};
			if (!playlist.find(item => item.videoId === video.videoId)) {
				playlist.push(video);
				renderPlaylist();
				savePlaylist();
				urlInput.value = ''; // Clear the input field
			} else {
				alert('This video is already in the playlist.');
			}
		} else {
			alert('Video not found.');
		}
	} else {
		alert('Invalid URL.');
	}
}

// Function to extract video ID from YouTube/Invidious URL
function extractVideoIdFromUrl(url) {
	const regex = /(?:https?:\/\/)?(?:www\.)?(?:youtube\.com\/(?:[^\/\n\s]+\/\S+\/|(?:v|e(?:mbed)?)\/|.*[?&]v=)|youtu\.be\/|invidio\.us\/watch\?v=)([a-zA-Z0-9_-]{11})/;
	const match = url.match(regex);
	return match ? match[1] : null;
}

// Attach event listener to the add URL button
document.getElementById('addUrlButton').addEventListener('click', addVideoByUrl);


const toggleAutoPlay = document.getElementById('toggleAutoPlay');
toggleAutoPlay.addEventListener('click', function() {
	// Toggle the boolean value
	autoPlay = !autoPlay;
	saveBoolean('autoPlay', autoPlay);
	// Change button color based on the boolean value
	if (autoPlay) {
		toggleAutoPlay.classList.remove('btn-danger');
		toggleAutoPlay.classList.add('btn-success');
		toggleAutoPlay.textContent = "Auto Play";
	} else {
		toggleAutoPlay.classList.remove('btn-success');
		toggleAutoPlay.classList.add('btn-danger');
		toggleAutoPlay.textContent = "Auto Play";
	}
});
// Function to fetch videos from Invidious API
async function searchVideos(query, page = 1) {
	const response = await fetch(`${invidiousApiUrl}/search?q=${query}&page=${page}&type=video&region=FR`);
	const data = await response.json();
	return data;
}
// Function to render video thumbnails
function renderVideoThumbnails(videos) {
	const videoResults = document.getElementById('videoResults');
	videoResults.innerHTML = '';
	videos.forEach(video => {
		const thumbnailUrl = video.videoThumbnails[4]?.url;
		const title = video.title;
		const videoId = video.videoId;
		const thumbnail = document.createElement('div');
		thumbnail.classList.add('col-md-4', 'video-thumbnail');
		thumbnail.innerHTML = `
		<div class="card mb-3 hover">
		<img src="${thumbnailUrl}" class="card-img-top" alt="${title}">
		<div class="card-body">
		<h5 class="card-title" title="${title}">${title}</h5>
		</div>
		</div>
		`;
		const card = thumbnail.querySelector('.card.mb-3');
		card.addEventListener('click', () => selectVideo(thumbnail, video));
		videoResults.appendChild(thumbnail);
	});
}


// Function to handle video selection
function selectVideo(thumbnail, video) {
	const cardElement = thumbnail.querySelector('.card.mb-3');
	if (thumbnail.classList.contains('selected')) {
		thumbnail.classList.remove('selected');
		cardElement.classList.remove('highlighted'); // Remove highlighted class
		selectedVideos = selectedVideos.filter(v => v.videoId !== video.videoId);
	} else {
		thumbnail.classList.add('selected');
		cardElement.classList.add('highlighted'); // Add highlighted class
		selectedVideos.push(video);
	}
}
// Function to add selected videos to playlist (end)
function addToPlaylist() {
	selectedVideos.forEach(video => {
		if (!playlist.find(item => item.videoId === video.videoId)) {
			playlist.push(video);
		}
	});
	renderPlaylist();
	savePlaylist();
	clearSelection();
}
// Function to add selected videos to the beginning of the playlist
function addToBeginning() {
	selectedVideos.forEach(video => {
		if (!playlist.find(item => item.videoId === video.videoId)) {
			playlist.unshift(video);
		}
	});
	renderPlaylist();
	savePlaylist();
	clearSelection();
}
// Function to render playlist
function renderPlaylist() {
	const playlistElement = document.getElementById('playlist');
	playlistElement.innerHTML = '';
	playlist.forEach((item, index) => {
		const listItem = document.createElement('li');
		listItem.classList.add('list-group-item', 'playlist-item');
		listItem.innerHTML = `
		<span class="playlist-title" title="${item.title}">${index + 1}. ${item.title}</span>
		`;
		playlistElement.appendChild(listItem);

		// Create container for delete button
		const buttonContainer = document.createElement('div');
		buttonContainer.classList.add('delete-button-container');

		// Create delete button
		const deleteButton = document.createElement('button');
		deleteButton.classList.add('btn', 'btn-danger', 'btn-sm');
		deleteButton.innerHTML = `
		<img src="trash-solid.svg" alt="Delete" width="16" height="16">
		`;
		deleteButton.addEventListener('click', () => removeFromPlaylist(index));

		// Append delete button to its container
		buttonContainer.appendChild(deleteButton);

		// Append delete button container to playlist item
		listItem.appendChild(buttonContainer);
	});
	// Initialize SortableJS for the playlist
	new Sortable(playlistElement, {
		animation: 150,
		onEnd: (event) => {
			const movedItem = playlist.splice(event.oldIndex, 1)[0];
			playlist.splice(event.newIndex, 0, movedItem);
			savePlaylist();
			renderPlaylist(); // Re-render to update the order
		}
	});
}



// Function to clear selection
function clearSelection() {
	selectedVideos = [];
	const thumbnails = document.querySelectorAll('.highlighted');
	thumbnails.forEach(thumbnail => thumbnail.classList.remove('highlighted'));
}
// Function to remove video from playlist
function removeFromPlaylist(index) {
	playlist.splice(index, 1);
	savePlaylist();
	renderPlaylist();
}

function playNextVideo() {
	if (playlist.length > 0) {
		const nextVideo = playlist.shift();
		currentlyPlaying = nextVideo;
		savePlaylist();
		renderPlaylist();
		// Send the next video to MPV
		sendToMPV(nextVideo).then(response => {
			// Parse the MPV response
			const responseData = JSON.parse(response);
			// Check if the response includes "error": "success"
			if (responseData.error === "success") {
				const progressSlider = document.getElementById("progressSlider");
				progressSliderPos = 0;
				progressSlider.value = progressSliderPos;
			} else {
				// Handle error if the response is not successful
				console.error('Error playing next video:', responseData.error);
			}
		}).catch(error => {
			console.error('Error playing next video:', error);
			// Handle error if there is an issue with the request
		});
		console.log(nextVideo);
	} else {
		// No video in the playlist, handle accordingly
	}
}
// Function to restart the current video
function restartVideo() {
	if (currentlyPlaying) {
		//document.getElementById('currentlyPlaying').textContent = `Now playing: ${currentlyPlaying.title}`;
		sendToMPV(currentlyPlaying);
	}
}
// Function to save playlist to localStorage
function savePlaylist() {
	localStorage.setItem('playlist', JSON.stringify(playlist));
}
// Function to clear the playlist
function clearPlaylist() {
	playlist = []; // Clear the playlist array
	savePlaylist(); // Save the empty playlist to localStorage
	renderPlaylist(); // Update the UI to reflect the changes
}
// Function to send video URL to MPV
function sendToMPV(video) {
	console.log(video);
	// Define the POST request body containing the video URL
	const requestBody = {
		command: ['loadfile', 'https://www.youtube.com/watch?v=' + video.videoId]
	};

	// Send POST request using sendCommand function
	sendCommand(requestBody)
	.then(() => {
		console.log('Command sent successfully to MPV');
	})
	.catch(error => {
		console.error('Error:', error);
	});
}


// Function to update playback time and total duration
function updatePlaybackInfo() {
	const apiUrl = apiUrlMPV + '/properties'; // URL of your Flask server's /properties endpoint

	// Make a single GET request to retrieve all properties
	fetch(apiUrl)
	.then(response => response.json())
	.then(data => {
		const timePos = data['playback-time'];
		const duration = data['duration'];
		const title = data['media-title'];
		const volume = data['volume'];
		const isIdle = data['idle-active'];
		const isPaused = data['pause'];

		// Convert playback time and duration from seconds to HH:MM:SS format
		const playbackTime = formatTime(timePos);
		const totalDuration = formatTime(duration);

		// Update HTML to display playback time, total duration, and video title
		document.getElementById('playbackInfo').innerHTML = `
		<strong>Playback:</strong> ${playbackTime} / ${totalDuration}<br>
		<strong>Title:</strong> ${title}
		`;


		// Get the progress slider element
		const progressSlider = document.getElementById("progressSlider");
		progressSlider.max = duration;

		if (progressSliderMoved) {
			setPlaybackTime(progressSliderPos);
			progressSliderMoved = false;
		} else {
			progressSlider.value = timePos;
		}


		if (volumeSliderMoved) {
			setVolume(volumeSliderPos);
			volumeSliderMoved = false;
		} else {
			const volumeSlider = document.getElementById('volumeSlider');
			volumeSlider.value = volume;
		}

		if (isIdle) {
			if (playlist.length > 0 && autoPlay) {
				// Play next video if playlist is not empty
				playNextVideo();
			}
		}
		if (!isPaused) {
			document.getElementById('pauseButton').textContent = 'Pause';
			videoWaitingPlay = false;
		} else {
			document.getElementById('pauseButton').textContent = 'Resume';
		}
	})
	.catch(error => {
		console.error('Error:', error);
		// Display error message in console
	});
}


// Function to format time from seconds to HH:MM:SS format
function formatTime(seconds) {
	const hours = Math.floor(seconds / 3600);
	const minutes = Math.floor((seconds % 3600) / 60);
	const remainingSeconds = Math.floor(seconds % 60);
	return `${hours.toString().padStart(2, '0')}:${minutes.toString().padStart(2, '0')}:${remainingSeconds.toString().padStart(2, '0')}`;
}
// Function to make a POST request
function sendCommand(requestBody) {
	const apiUrl = apiUrlMPV + '/command';
	// Define POST request options
	const requestOptions = {
		method: 'POST',
		headers: {
			'Content-Type': 'application/json'
		},
		body: JSON.stringify(requestBody)
	};
	// Send POST request and return response JSON
	return fetch(apiUrl, requestOptions).then(response => {
		if (!response.ok) {
			throw new Error('Failed to send request');
		}
		return response.json(); // Parse response JSON
	});
}
// Call updatePlaybackInfo function every 2 seconds
setInterval(updatePlaybackInfo, 1000);
// Function to send a pause or resume command to MPV
function togglePause() {
	// Define current state of playback
	let pauseState = document.getElementById('pauseButton').textContent.trim().toLowerCase() === 'pause';
	// Define command based on current state
	const command = pauseState ? ['set_property', 'pause', true] : ['set_property', 'pause', false];
	// Send command to MPV
	sendCommand({
		command: command
	}).then(() => {
		// Update button text based on new state
		document.getElementById('pauseButton').textContent = pauseState ? 'Resume' : 'Pause';
	}).catch(error => {
		console.error('Error:', error);
		// Display error message in console
	});
}
// Attach click event listener to the pause button
document.getElementById('pauseButton').addEventListener('click', togglePause);
// Function to set volume
function setVolume(volume) {
	// Send volume command to MPV
	sendCommand({
		command: ['set_property', 'volume', volume]
	}).catch(error => {
		console.error('Error:', error);
		// Display error message in console
	});
}
// Attach click event listener to the pause button
document.getElementById('pauseButton').addEventListener('click', togglePause);

// Attach input event listener to the volume slider
document.getElementById('volumeSlider').addEventListener("input", function() {
	volumeSliderPos = document.getElementById('volumeSlider').value;
	volumeSliderMoved = true;
});


document.addEventListener('DOMContentLoaded', function() {
	// Load playlist from localStorage
	playlist = JSON.parse(localStorage.getItem('playlist')) || [];
	// Render the playlist
	renderPlaylist();
});
// Function to refresh the playlist
function refreshPlaylist() {
	playlist = JSON.parse(localStorage.getItem('playlist')) || []; // Reload playlist from localStorage
	renderPlaylist(); // Update the UI to reflect the changes
}
// Function to perform search
async function performSearch() {
	const searchInput = document.getElementById('searchInput');
	searchQuery = searchInput.value.trim();
	if (searchQuery !== '') {
		currentPage = 1;
		const videos = await searchVideos(searchQuery, currentPage);
		renderVideoThumbnails(videos);
	}
}
// Event listener for input event on search input
document.getElementById('searchInput').addEventListener('input', async (event) => {
	await performSearch();
});
// Event listener for keydown event on search input
document.getElementById('searchInput').addEventListener('keydown', async (event) => {
	if (event.key === 'Enter') {
		await performSearch();
	}
});
// Event listener for "Add to Playlist" button
document.getElementById('addToPlaylistButton').addEventListener('click', addToPlaylist);
// Event listener for "Add to Beginning" button
document.getElementById('addToBeginningButton').addEventListener('click', addToBeginning);
// Event listener for "Previous" button
document.getElementById('prevButton').addEventListener('click', async () => {
	if (currentPage > 1) {
		currentPage--;
		const videos = await searchVideos(searchQuery, currentPage);
		renderVideoThumbnails(videos);
	}
});
// Event listener for "Next" button
document.getElementById('nextButton').addEventListener('click', async () => {
	currentPage++;
	const videos = await searchVideos(searchQuery, currentPage);
	renderVideoThumbnails(videos);
});
// Event listeners for the fixed header buttons
document.getElementById('playNextVideoButton').addEventListener('click', playNextVideo);
document.getElementById('restartVideoButton').addEventListener('click', restartVideo);
// Event listener for "Clear Playlist" button
document.getElementById('clearPlaylistButton').addEventListener('click', clearPlaylist);
// Event listener for "Refresh Playlist" button
document.getElementById('refreshPlaylistButton').addEventListener('click', refreshPlaylist);

document.getElementById('progressSlider').addEventListener("input", function() {
	progressSliderPos = document.getElementById('progressSlider').value;
	progressSliderMoved = true;
});



function setPlaybackTime(playbackTime) {
	// Send playbackTime command to MPV
	sendCommand({
		command: ['set_property', 'playback-time', playbackTime]
	}).catch(error => {
		console.error('Error:', error);
		// Display error message in console
	});
}
