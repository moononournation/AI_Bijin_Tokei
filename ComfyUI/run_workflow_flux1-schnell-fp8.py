import json
from urllib import request
import random
from pathlib import Path
from PIL import Image

SERVER_ADDRESS = "127.0.0.1:8188"
API_URL = f"http://{SERVER_ADDRESS}/prompt"

BASE_DIR = Path(__file__).resolve().parent
JSON_FILE_PATH = BASE_DIR / "flux1-schnell-fp8.json"
PNG_DIR = BASE_DIR / "png"
JPG_LIST = [
    {"DIR": BASE_DIR / "jpg320", "SIZE": (320, 320), "QUALITY": 65},
    # {"DIR": BASE_DIR / "jpg480", "SIZE": (480, 480), "QUALITY": 90},
    # {"DIR": BASE_DIR / "jpg1024", "SIZE": (1024, 1024), "QUALITY": 90},
]

# int32 max - reserve 4 minutes interval top up
seed_base = random.randint(1, 4294967295 - (1440 // 4))


# 1. read JSON
def load_workflow(filepath):
    with open(filepath, "r", encoding="utf-8") as f:
        return json.load(f)


# 2. change parameters
filename_prefix = ""

poseList = list()
poseList.append(
    "She smiles gently and holds the small rectangular blackboard with both hands at chest height, keeping it level and facing the camera."
)
poseList.append(
    "She smiled warmly at the camera while holding up a small rectangular blackboard at a tilted angle above her head with one hand."
)
poseList.append(
    "She smiled naturally at the camera while holding a small rectangular blackboard up at right side next to her face with both hands."
)
poseList.append(
    "She tilts her head slightly, smiles playfully at the camera while holding a small rectangular blackboard up at the left side next to her face with one hand."
)

sunrisePrompt = "Portrait of an young Japanese woman outdoors at sunrise. [pose] The blackboard displays [time] in large, clear white chalk handwriting, with small chalk drawings of a rising sun in the corners. The writing is centered, unobstructed, and easy to read. She wears a light jacket over a modest casual outfit. Soft peach and gold dawn light, calm sky, natural skin texture, gently blurred background."
morningPrompt = "Portrait of an young Japanese woman in a bright green park in the morning. [pose] The blackboard displays [time] in large, clear white chalk handwriting, with small chalk drawings of leaves and a tiny sun in the corners. The writing is centered, unobstructed, and easy to read. Neat casual outfit, fresh morning daylight, soft shadows, leafy background gently blurred, natural skin texture."
middayPrompt = "Portrait of an young Japanese woman outdoors at midday on a bright, clear day. [pose] The blackboard displays [time] in large, clear white chalk handwriting, with a small chalk sun drawing in one corner. The writing is centered, unobstructed, and easy to read. Light modest seasonal clothing, bright natural daylight, clear colors, softly blurred park background."
afternoonPrompt = "Portrait of an young Japanese woman in a quiet park in the afternoon. [pose] The blackboard displays [time] in large, clear white chalk handwriting, with small chalk drawings of leaves and a tiny teacup in the corners. The writing is centered, unobstructed, and easy to read. Stylish casual outfit, warm afternoon sunlight, soft shadows, gently blurred background."
sunsetPrompt = "Portrait of an young Japanese woman outdoors at sunset. [pose] The blackboard displays [time] in large, clear white chalk handwriting, with small chalk drawings of a setting sun and clouds in the corners. The writing is centered, unobstructed, and easy to read. Light coat or cardigan, warm orange and pink evening light, softly blurred sky, her face and the blackboard clearly visible."
nightPrompt = "Portrait of an young Japanese woman on a softly lit city street at night. [pose] The blackboard displays [time] in large, clear white chalk handwriting, with small chalk drawings of stars and a crescent moon in the corners. The writing is centered, unobstructed, and easy to read. Neat evening outfit, softly blurred shop and city lights, her face and the blackboard clearly lit."
midnight = "Portrait of an young Japanese woman beside a quiet city street near midnight. [pose] The blackboard displays [time] in large, clear white chalk handwriting, with small chalk drawings of a crescent moon and stars in the corners. The writing is centered, unobstructed, and easy to read. Deep blue night tones, a few distant blurred lights, soft light on her face and the blackboard."


def gen_prompt(wf, h, m):
    wf["31"]["inputs"]["seed"] = seed_base + (60 * h) + (m // 4)

    prompt = ""
    if 5 <= h < 7:
        prompt = sunrisePrompt
    elif 7 <= h < 11:
        prompt = morningPrompt
    elif 11 <= h < 13:
        prompt = middayPrompt
    elif 13 <= h < 17:
        prompt = afternoonPrompt
    elif 17 <= h < 19:
        prompt = sunsetPrompt
    elif 19 <= h < 23:
        prompt = nightPrompt
    else:
        prompt = midnight

    prompt = prompt.replace("[pose]", poseList[m % 4])

    wf["9"]["inputs"]["filename_prefix"] = f"{filename_prefix}-{h:02d}-{m:02d}"
    wf["6"]["inputs"]["text"] = prompt.replace("[time]", f"'{h:02d} : {m:02d}'")
    # print(wf["6"]["inputs"]["text"])
    queue_prompt(wf)


# 3. queue ComfyUI
def queue_prompt(workflow_dict):
    payload = {"prompt": workflow_dict}
    data = json.dumps(payload).encode("utf-8")

    req = request.Request(
        API_URL, data=data, headers={"Content-Type": "application/json"}
    )

    with request.urlopen(req) as response:
        result = json.loads(response.read().decode("utf-8"))
        print(f"Queued Prompt ID: {result.get('prompt_id')}")


# 4. convert images to different sizes
def convert_image(h, m, files):
    for jpg_info in JPG_LIST:
        output_path = jpg_info["DIR"] / f"{h:02d}/{m:02d}"
        Path(output_path).mkdir(parents=True, exist_ok=True)

    for file in files:
        missing_file = False
        for jpg_info in JPG_LIST:
            jpg_path = jpg_info["DIR"] / f"{h:02d}/{m:02d}" / f"{file.stem}.jpg"
            missing_file = missing_file or not jpg_path.exists()

        if missing_file:
            print(f"converting: {file.name}")
            img = Image.open(file)
            for jpg_info in JPG_LIST:
                jpg_path = jpg_info["DIR"] / f"{h:02d}/{m:02d}" / f"{file.stem}.jpg"
                if not jpg_path.exists():
                    resized_img = img.resize(
                        jpg_info["SIZE"]
                    )  # Resize the image to the specified size
                    resized_img.save(
                        jpg_path, "JPEG", quality=jpg_info["QUALITY"]
                    )  # Save the resized image as JPEG
                    resized_img.close()
            img.close()


if __name__ == "__main__":
    wf = load_workflow(JSON_FILE_PATH)
    filename_prefix = wf["9"]["inputs"]["filename_prefix"]

    # loop enqueue all minutes
    for h in range(0, 24):
        for m in range(0, 60):
            file_pattern = f"{filename_prefix}-{h:02d}-{m:02d}*.png"
            files = list(PNG_DIR.glob(file_pattern))
            if len(files) > 0:
                convert_image(h, m, files)
            else:
                print(f"no files found for {file_pattern}")
                # gen_prompt(wf, h, m)
