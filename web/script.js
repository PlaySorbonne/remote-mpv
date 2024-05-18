document.getElementById('hamburgerMenu').addEventListener('click', function() {
    this.classList.toggle('open');
    document.getElementById('playlist').classList.toggle('open');
});


document.addEventListener('DOMContentLoaded', function() {
    let currentPage = 1;
    let currentQuery = '';
    let playlist = [];
    let currentResults = [];

    function fetchResults(query, page) {
        fetch(`https://invidious.fdn.fr/api/v1/search?q=${encodeURIComponent(query)}&type=video&page=${page}`)
            .then(response => {
                if (!response.ok) {
                    throw new Error(`HTTP error! status: ${response.status}`);
                }
                return response.json();
            })
            .then(data => {
                if (!Array.isArray(data)) {
                    throw new Error('API response does not contain a valid array');
                }
                currentResults = data;
                const grid = document.getElementById('grid');
                grid.innerHTML = '';

                currentResults.forEach((item, index) => {
                    const gridItem = document.createElement('div');
                    gridItem.className = 'grid-item';
                    gridItem.setAttribute('data-index', index);
                    gridItem.innerHTML = `
                        <img src="${item.videoThumbnails[4].url}" alt="${item.title}">
                        <h3>${item.title}</h3>
                    `;
                    grid.appendChild(gridItem);

                    gridItem.addEventListener('click', function() {
                        gridItem.classList.toggle('selected');
                    });
                });

                document.getElementById('prevButton').disabled = page <= 1;
            })
            .catch(error => {
                console.error('Error making API call:', error);
                const grid = document.getElementById('grid');
                grid.innerHTML = `<p style="color: red;">Error: ${error.message}</p>`;
            });
    }

    function clearSelection() {
        document.querySelectorAll('.grid-item.selected').forEach(item => {
            item.classList.remove('selected');
        });
    }

    document.getElementById('searchForm').addEventListener('submit', function(event) {
        event.preventDefault();
        currentQuery = document.getElementById('searchInput').value;
        currentPage = 1;
        fetchResults(currentQuery, currentPage);
    });

    document.getElementById('prevButton').addEventListener('click', function() {
        if (currentPage > 1) {
            currentPage--;
            fetchResults(currentQuery, currentPage);
        }
    });

    document.getElementById('nextButton').addEventListener('click', function() {
        currentPage++;
        fetchResults(currentQuery, currentPage);
    });

    document.getElementById('addToPlaylist').addEventListener('click', function() {
        const selectedItems = document.querySelectorAll('.grid-item.selected');
        selectedItems.forEach(item => {
            const index = item.getAttribute('data-index');
            const video = currentResults[index];
            if (!playlist.some(vid => vid.videoId === video.videoId)) {
                playlist.push(video);
            }
        });

        populatePlaylist();
        clearSelection();
    });

    document.getElementById('addOnTopPlaylist').addEventListener('click', function() {
        const selectedItems = document.querySelectorAll('.grid-item.selected');
        const selectedVideos = Array.from(selectedItems).map(item => {
            const index = parseInt(item.getAttribute('data-index'));
            return currentResults[index];
        });
        playlist = [...selectedVideos, ...playlist]; // Add selected items to the beginning of the playlist
        populatePlaylist();

        clearSelection();
    });

    document.getElementById('playNextHeaderButton').addEventListener('click', function() {
        if (playlist.length > 0) {
            const nextVideo = playlist.shift();
            displayCurrentlyPlaying(nextVideo);
            playVideo(nextVideo);
            populatePlaylist();
        } else {
            alert('Playlist is empty!');
        }
    });

    document.getElementById('restartVideoButton').addEventListener('click', function() {
        if (currentlyPlayingVideo) {
            playVideo(currentlyPlayingVideo);
        } else {
            alert('No video is currently playing!');
        }
    });

    let currentlyPlayingVideo;

    function playVideo(video) {
        console.log(video);
        currentlyPlayingVideo = video;
    }

    function displayCurrentlyPlaying(video) {
        const currentlyPlayingDiv = document.getElementById('currentlyPlaying');
        currentlyPlayingDiv.innerHTML = `
            <h4>${video.title}</h4>
        `;
    }

    function createPlaylistItem(item, index) {
        const listItem = document.createElement('li');
        listItem.innerHTML = `
            <img src="${item.videoThumbnails[5].url}" alt="${item.title}">
            <h4>${item.title}</h4>
            <button class="moveUpButton" ${index === 0 ? 'disabled' : ''}>&uarr;</button>
            <button class="moveDownButton" ${index === playlist.length - 1 ? 'disabled' : ''}>&darr;</button>
            <button class="removeItemButton">Remove Item</button>
        `;
        listItem.querySelector('.removeItemButton').addEventListener('click', function() {
            playlist.splice(index, 1);
            listItem.remove();
            populatePlaylist();
        });

        listItem.querySelector('.moveUpButton').addEventListener('click', function() {
            if (index > 0) {
                const movedItem = playlist[index];
                playlist.splice(index, 1);
                playlist.splice(index - 1, 0, movedItem);
                populatePlaylist();
            }
        });

        listItem.querySelector('.moveDownButton').addEventListener('click', function() {
            if (index < playlist.length - 1) {
                const movedItem = playlist[index];
                playlist.splice(index, 1);
                playlist.splice(index + 1, 0, movedItem);
                populatePlaylist();
            }
        });

        return listItem;
    }

    function populatePlaylist() {
        const playlistContainer = document.getElementById('playlistItems');
        playlistContainer.innerHTML = '';

        playlist.forEach((item, index) => {
            const listItem = createPlaylistItem(item, index);
            playlistContainer.appendChild(listItem);
        });
    }

    document.getElementById('clearPlaylistButton').addEventListener('click', function() {
        const playlistContainer = document.getElementById('playlistItems');
        playlistContainer.innerHTML = '';
        playlist = [];
    });

});
