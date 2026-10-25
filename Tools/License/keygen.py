#!/usr/bin/env python3
"""African Drum licence key generator.
   AG_LICENSE_SECRET=<the secret the plugin was built with> python3 keygen.py "Customer Name" customer@email.com"""
import hashlib, hmac, os, sys

if len(sys.argv) != 3 or not os.environ.get("AG_LICENSE_SECRET"):
    sys.exit(__doc__)

name, email = sys.argv[1], sys.argv[2]
msg = f"{name.strip().lower()}|{email.strip().lower()}"
hexd = hmac.new(os.environ["AG_LICENSE_SECRET"].encode(), msg.encode(), hashlib.sha256).hexdigest().upper()[:25]
print("AGD-" + "-".join(hexd[i:i + 5] for i in range(0, 25, 5)))
