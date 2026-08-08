from setuptools import find_packages, setup

package_name = "quorum_injector"

setup(
    name=package_name,
    version="0.0.0",
    packages=find_packages(exclude=["test"]),
    data_files=[
        ("share/ament_index/resource_index/packages", ["resource/" + package_name]),
        ("share/" + package_name, ["package.xml"]),
    ],
    install_requires=["setuptools"],
    tests_require=["pytest"],
    zip_safe=True,
    maintainer="borja",
    maintainer_email="borja@borjadelamora.com",
    description="Relays the sensor topics the supervision layer reads, and corrupts them on demand",
    license="MIT",
    entry_points={
        "console_scripts": ["relay_node = quorum_injector.relay_node:main"],
    },
)
