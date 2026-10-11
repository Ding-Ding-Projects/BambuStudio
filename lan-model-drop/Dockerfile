# LAN model drop: a small web page on the local network where other people
# send 3D models to Bambu Studio MD3 on this computer. Node.js built-ins
# only; nothing is installed from a package registry.
#
# The base image is pinned by tag and by the digest of its multi-platform
# index, so every build starts from exactly the same image.
FROM node:22.23.3-alpine3.24@sha256:0a7108bf6c7bf5de370ffb1a3ed6be93d405b43ff159f681a8d18c0e2bc2e402

ENV NODE_ENV=production \
    DROP_DATA_DIR=/data

WORKDIR /app

# The program belongs to root and is only readable by the service user, so
# the running service cannot change itself.
COPY --chown=root:root --chmod=0755 server/ ./server/
COPY --chown=root:root --chmod=0755 site/ ./site/

# The data folder (station key, drop code, queued models) is the only place
# the service writes. A new named volume takes this owner and mode.
RUN mkdir -p /data && chown node:node /data && chmod 0700 /data

# The node user (uid 1000) that the base image provides.
USER node

EXPOSE 8080
VOLUME ["/data"]

HEALTHCHECK --interval=30s --timeout=5s --start-period=15s --retries=3 \
    CMD ["node", "server/healthcheck.mjs"]

STOPSIGNAL SIGTERM
CMD ["node", "server/main.mjs"]
