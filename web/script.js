const form = document.getElementById('myForm');
const resultList = document.getElementById('resultList');

// Function to recursively generate HTML elements for nested objects
function generateNestedTree(data) {
    const ul = document.createElement('ul');
    if (Array.isArray(data)) {
        data.forEach(item => {
            const li = document.createElement('li');
            li.appendChild(generateNestedTree(item));
            ul.appendChild(li);
        });
    } else if (typeof data === 'object' && data !== null) {
        for (const key in data) {
            const li = document.createElement('li');
            li.textContent = key;
            li.appendChild(generateNestedTree(data[key]));
            ul.appendChild(li);
        }
    } else {
        const li = document.createElement('li');
        li.textContent = data;
        ul.appendChild(li);
    }
    return ul;
}

form.addEventListener('submit', async function(event) {
    event.preventDefault();

    // Get command value
    const commandInput = document.getElementById('command');
    const command = commandInput.value;

    // Create JSON object with command as an array
    const jsonData = { command: JSON.parse(command) };

    try {
        const response = await fetch('http://localhost:8080/post', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(jsonData)
        });

        if (!response.ok) {
            const errorMessage = await response.text();
            console.error('Error:', errorMessage);
            return;
        }

        const responseData = await response.json();

        console.log('responseData:', responseData); // Log responseData

        // Clear previous results
        resultList.innerHTML = '';

        // Display response data as a nested tree
        const li = document.createElement('li');
        li.appendChild(generateNestedTree(responseData));
        resultList.appendChild(li);

    } catch (error) {
        console.error('Error:', error);
    }
});
