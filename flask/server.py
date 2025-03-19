from flask import Flask, request, jsonify, render_template, session, redirect, url_for
from flask_cors import CORS
from pymongo import MongoClient
from datetime import datetime
import pytz
import random

app = Flask(__name__)
app.secret_key = "supersecretkey"  # Secret key for session management
CORS(app)  # Enable CORS for frontend access

# Connect to MongoDB
client = MongoClient("mongodb://localhost:27017/")
db = client["security_dashboard"]
collection_status = db["m5stick_status"]
collection_users = db["m5stick_user_particulars"]

# Set Singapore Timezone
SGT = pytz.timezone("Asia/Singapore")

# Expanded Elderly & Caregiver Name Lists
ELDERLY_NAMES = [
    "Tan Ah Kow", "Lim Beng Hock", "Ng Mei Ling", "Ong Siew Hoon", "Chong Ah Leong",
    "Foo Ah Kow", "Lee Bee Hua", "Goh Cheng Siong", "Halimah Yusof", "Raj Kumar",
    "Thomas Ng", "Sophia Tan", "Muthu Rajan", "Ali Bin Hassan", "Jessica Lim"
]

CAREGIVER_NAMES = [
    "John Lim", "Melissa Tan", "Raymond Chua", "Elena Koh", "David Ong",
    "Stephanie Ng", "Jason Teo", "Michelle Wong", "Ahmad Ismail", "Vincent Lau",
    "Sharon Ho", "Daniel Goh", "Rachel Chong", "Samuel Toh", "Farah Binte Yusoff"
]

BASE_BLOCK = "570"
BASE_POSTAL = "560570"

def generate_random_unit():
    """ Generate a random Singapore unit number (e.g., #04-3341) """
    unit_floor = random.randint(2, 25)  # Floor range
    unit_number = random.randint(1000, 3999)  # Random unit within floor
    return f"#{unit_floor:02d}-{unit_number}"

def generate_random_phone():
    """ Generate a random Singapore phone number """
    return f"+65 9{random.randint(1000000, 9999999)}"

def generate_random_email(name, id):
    """ Generate a simple random caregiver email based on name & id """
    email_username = name.lower().replace(" ", ".") + str(id)
    return f"{email_username}@gmail.com"

def generate_random_password():
    """ Generate a simple random password """
    return f"P@ss{random.randint(1000, 9999)}"

# ✅ API for User Login
@app.route("/login", methods=["POST"])
def login():
    try:
        data = request.json
        email = data.get("email")
        password = data.get("password")

        user = collection_users.find_one({"caregiver_email": email, "caregiver_password": password}, {"_id": 0})

        if user:
            session["user"] = user
            return jsonify({"message": "Login successful", "role": user["role"], "id": user["id"]}), 200
        else:
            return jsonify({"error": "Invalid email or password"}), 401

    except Exception as e:
        return jsonify({"error": str(e)}), 500

# ✅ API to Logout
@app.route("/logout", methods=["GET"])
def logout():
    session.pop("user", None)
    return redirect(url_for("login_page"))

# ✅ API to Update Node Status
@app.route("/update_status", methods=["POST"])
def update_status():
    try:
        data = request.json  
        if not data or "id" not in data or "status" not in data:
            return jsonify({"error": "Invalid data format"}), 400
        
        device_id = data["id"]
        status = data["status"]
        timestamp = datetime.now(SGT).strftime("%H:%M:%S %d/%m/%y")

        # Insert new entry instead of updating the same document
        collection_status.insert_one({
            "id": device_id,
            "status": status,
            "timestamp": timestamp
        })

        # Check if user particulars already exist for this ID
        if not collection_users.find_one({"id": device_id}):
            print(f"New node detected ({device_id}) - Generating user particulars...")

            # Generate Random User Particulars
            elderly_name = random.choice(ELDERLY_NAMES)
            elderly_address = f"{generate_random_unit()}, {BASE_BLOCK}, {BASE_POSTAL}"
            caregiver_name = random.choice(CAREGIVER_NAMES)
            caregiver_phone = generate_random_phone()
            caregiver_email = generate_random_email(caregiver_name, device_id)
            caregiver_password = generate_random_password()

            # Insert into user particulars collection
            collection_users.insert_one({
                "id": device_id,
                "elderly_name": elderly_name,
                "elderly_address": elderly_address,
                "caregiver_name": caregiver_name,
                "caregiver_phone": caregiver_phone,
                "caregiver_email": caregiver_email,
                "caregiver_password": caregiver_password,
                "role": "caregiver"  # Assign caregiver role
            })

            print(f"User particulars created for node {device_id}")

        return jsonify({"message": "Status updated", "id": device_id, "timestamp": timestamp}), 200

    except Exception as e:
        return jsonify({"error": str(e)}), 500

# ✅ API to Get Latest Status (Fixed for Caregivers)
@app.route("/get_latest_status", methods=["GET"])
def get_latest_status():
    if "user" not in session:
        return jsonify({"error": "Unauthorized"}), 401

    try:
        user = session["user"]

        pipeline = [
            {"$sort": {"timestamp": -1}},  
            {"$group": {"_id": "$id", "latest_status": {"$first": "$status"}, "timestamp": {"$first": "$timestamp"}}}
        ]

        latest_data = list(collection_status.aggregate(pipeline))

        if user["role"] == "caregiver":
            latest_data = [node for node in latest_data if node["_id"] == user["id"]]

        return jsonify(latest_data), 200

    except Exception as e:
        return jsonify({"error": str(e)}), 500


# ✅ API to Get Node History (Caregiver Restriction Applied)
@app.route("/get_history/<node_id>", methods=["GET"])
def get_history(node_id):
    if "user" not in session:
        return jsonify({"error": "Unauthorized"}), 401

    user = session["user"]

    if user["role"] == "caregiver" and user["id"] != node_id:
        return jsonify({"error": "Unauthorized access"}), 403

    try:
        history = list(collection_status.find({"id": node_id}, {"_id": 0}).sort("timestamp", -1))
        return jsonify(history), 200
    except Exception as e:
        return jsonify({"error": str(e)}), 500

# ✅ API to Get User Particulars Based on Role
@app.route("/get_user_particulars/<node_id>", methods=["GET"])
def get_user_particulars(node_id):
    if "user" not in session:
        return jsonify({"error": "Unauthorized"}), 401

    user = session["user"]
    user_data = collection_users.find_one({"id": node_id}, {"_id": 0})

    if not user_data:
        return jsonify({"error": "User details not found"}), 404

    if user["role"] == "admin":
        return jsonify(user_data), 200
    elif user["role"] == "caregiver" and user["id"] == node_id:
        limited_data = {
            "id": user_data["id"],
            "elderly_name": user_data["elderly_name"],
            "elderly_address": user_data["elderly_address"]
        }
        return jsonify(limited_data), 200
    else:
        return jsonify({"error": "Unauthorized access"}), 403

# ✅ API to Get Caregiver Particulars (Admin and Caregiver view)
@app.route("/get_caregiver_particulars/<node_id>", methods=["GET"])
def get_caregiver_particulars(node_id):
    if "user" not in session:
        return jsonify({"error": "Unauthorized"}), 401

    user = session["user"]
    user_data = collection_users.find_one({"id": node_id}, {"_id": 0})

    if not user_data:
        return jsonify({"error": "User details not found"}), 404

    if user["role"] == "admin":
        caregiver_data = {
            "caregiver_name": user_data["caregiver_name"],
            "caregiver_phone": user_data["caregiver_phone"],
            "caregiver_email": user_data["caregiver_email"]
        }
        return jsonify(caregiver_data), 200
    elif user["role"] == "caregiver" and user["id"] == node_id:
        caregiver_data = {
            "caregiver_name": user_data["caregiver_name"],
            "caregiver_phone": user_data["caregiver_phone"]
        }
        return jsonify(caregiver_data), 200
    else:
        return jsonify({"error": "Unauthorized access"}), 403
    
# ✅ Serve Login Page
@app.route("/login_page")
def login_page():
    return render_template("login.html")

# ✅ Serve Dashboard Page (Requires Login)
@app.route("/")
def dashboard():
    if "user" not in session:
        return redirect(url_for("login_page"))
    user_role = session["user"].get("role")  # Get user role from the session
    return render_template("dashboard.html", user_role=user_role)

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)