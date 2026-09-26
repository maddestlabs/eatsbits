// Eatsbits Modular Workstation - Offline PWA Service Worker
const CACHE_NAME = 'eatsbits-pwa-v1';

const PRECACHE_ASSETS = [
    './',
    './index.html',
    './eatsbits.html',
    './eatsbits.js',
    './eatsbits.wasm',
    './eatsbits.data',
    './app_icon.svg',
    './icon-192.png',
    './icon-512.png',
    './icon-maskable-192.png',
    './icon-maskable-512.png',
    './apple-touch-icon.png',
    './favicon.ico',
    './manifest.json'
];

// Helper to ensure required security headers (COOP/COEP) for multi-threaded WASM
function ensureCoopCoepHeaders(response) {
    if (!response || response.type === 'opaque') {
        return response;
    }
    const headers = new Headers(response.headers);
    let modified = false;

    if (!headers.get('Cross-Origin-Opener-Policy')) {
        headers.set('Cross-Origin-Opener-Policy', 'same-origin');
        modified = true;
    }
    if (!headers.get('Cross-Origin-Embedder-Policy')) {
        headers.set('Cross-Origin-Embedder-Policy', 'require-corp');
        modified = true;
    }

    if (modified) {
        return new Response(response.body, {
            status: response.status,
            statusText: response.statusText,
            headers: headers
        });
    }
    return response;
}

// Install Event: Precaches all core DAW binaries and UI assets
self.addEventListener('install', (event) => {
    event.waitUntil(
        caches.open(CACHE_NAME).then(async (cache) => {
            console.log('[SW] Pre-caching Eatsbits offline assets...');
            // Fetch and cache sequentially with error resilience so one optional asset failure doesn't block the rest
            for (const asset of PRECACHE_ASSETS) {
                try {
                    const response = await fetch(asset);
                    if (response.ok) {
                        await cache.put(asset, response);
                    }
                } catch (err) {
                    console.warn(`[SW] Could not pre-cache ${asset} during install:`, err);
                }
            }
        }).then(() => self.skipWaiting())
    );
});

// Activate Event: Cleans up obsolete cache versions and claims clients immediately
self.addEventListener('activate', (event) => {
    event.waitUntil(
        caches.keys().then((keys) => {
            return Promise.all(
                keys.map((key) => {
                    if (key !== CACHE_NAME) {
                        console.log('[SW] Evicting outdated cache:', key);
                        return caches.delete(key);
                    }
                })
            );
        }).then(() => self.clients.claim())
    );
});

// Fetch Event: Offline-first with network fallback & COOP/COEP preservation
self.addEventListener('fetch', (event) => {
    const request = event.request;

    // Only handle GET requests
    if (request.method !== 'GET') {
        return;
    }

    const url = new URL(request.url);

    // Only cache same-origin resources
    if (url.origin !== self.location.origin) {
        return;
    }

    // HTML Navigation requests: Network-first falling back to cache
    if (request.mode === 'navigate' || request.destination === 'document') {
        event.respondWith(
            fetch(request)
                .then((networkResponse) => {
                    if (networkResponse && networkResponse.ok) {
                        const responseClone = networkResponse.clone();
                        caches.open(CACHE_NAME).then((cache) => {
                            cache.put(request, responseClone);
                        });
                    }
                    return ensureCoopCoepHeaders(networkResponse);
                })
                .catch(async () => {
                    console.log('[SW] Network unavailable, serving cached navigation page for:', request.url);
                    const cachedResponse = await caches.match(request) ||
                                           await caches.match('./eatsbits.html') ||
                                           await caches.match('./index.html');
                    if (cachedResponse) {
                        return ensureCoopCoepHeaders(cachedResponse);
                    }
                    return new Response('Eatsbits is offline and required assets are loading.', {
                        status: 503,
                        statusText: 'Offline',
                        headers: { 'Content-Type': 'text/plain' }
                    });
                })
        );
        return;
    }

    // Static assets (.wasm, .data, .js, .svg, .png, .ico, .json): Cache-first
    event.respondWith(
        caches.match(request).then((cachedResponse) => {
            if (cachedResponse) {
                // If found in cache, return immediately (instant offline load)
                return ensureCoopCoepHeaders(cachedResponse);
            }

            // Not in cache, fetch from network and populate cache
            return fetch(request).then((networkResponse) => {
                if (networkResponse && networkResponse.ok) {
                    const responseClone = networkResponse.clone();
                    caches.open(CACHE_NAME).then((cache) => {
                        cache.put(request, responseClone);
                    });
                }
                return ensureCoopCoepHeaders(networkResponse);
            }).catch((err) => {
                console.warn('[SW] Fetch failed for:', request.url, err);
                return cachedResponse;
            });
        })
    );
});
