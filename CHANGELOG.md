# Changelog

## [2026.9.1](https://github.com/seuros/meridian/compare/v2026.9.0...v2026.9.1) (2026-09-11)


### Fixes

* **build:** apply the state machine freestanding profile package-wide ([1cdee8c](https://github.com/seuros/meridian/commit/1cdee8c6c1791b69247fb7594b83cf1655a3d8d8))
* **build:** compile with -nostdlibinc so host headers stay out ([2bfa74a](https://github.com/seuros/meridian/commit/2bfa74a28a95cb7ad23bbc037978d6fb829456af))
* **build:** give Meridian its own weak mem* intrinsics ([29fd55b](https://github.com/seuros/meridian/commit/29fd55b1a89d3f8a01f4da8d39fcdaff269c120a))
* **build:** shim signal.h so Lua stops reaching into host glibc ([de40dce](https://github.com/seuros/meridian/commit/de40dce8df34484e604f53e6c52688e06737636a))
* **ci:** fetch the submodules build.py validates but never builds ([5981f14](https://github.com/seuros/meridian/commit/5981f1484136f42241abe9d908612691fe0ee99c))
* **ci:** let release-please recognise its own release PR ([1233c4d](https://github.com/seuros/meridian/commit/1233c4d897dc4ea799bb07a902a26d7e34e230f6))


### Build

* pin the clang toolchain in mise instead of apt ([2febd4f](https://github.com/seuros/meridian/commit/2febd4f3054de80ec2ee770689c59f042ab0392e))

## [2026.9.0](https://github.com/seuros/meridian/compare/v2026.6.0...v2026.9.0) (2026-09-10)


### Features

* add config/, the escape hatch, not the interface ([b41b8d3](https://github.com/seuros/meridian/commit/b41b8d3831f2f0deeabf52c39b1fe0238842e97c))
* add conn/, cherry picked from the future, where the interface is post apocalyptic ([d3370ff](https://github.com/seuros/meridian/commit/d3370ff7fb533bf637eedf343e2712acff3c3e57))
* add discovery/, the module the complexity merchants would rather you not find ([2709b3e](https://github.com/seuros/meridian/commit/2709b3eaa33f53897b37e92e3ae427ef85160506))
* add kernel/, just a hobby, won't be big and professional ([a7f6b54](https://github.com/seuros/meridian/commit/a7f6b54afa980b19acc81572f21ee956e689cd84))
* add lib/, where we admit what a boot manager mostly is ([0d17d38](https://github.com/seuros/meridian/commit/0d17d3875be8cd03ea7f22a9720aed40f74e1d05))
* add loaders/, minus the corpse, minus the IBM 5100 ([549d0c9](https://github.com/seuros/meridian/commit/549d0c92c2e4761438429aa0529ed66a00c23cca))
* add platform/, the wall of shame ([85b0c4a](https://github.com/seuros/meridian/commit/85b0c4a72b72834649887199231e2cfffda5a9f9))
* add storage/, because yes, I can still afford storage ([670aa6c](https://github.com/seuros/meridian/commit/670aa6c3e2311494b0c900de8e47a27886395ace))
* add ui/, the part that puts it on an actual screen ([1ed9011](https://github.com/seuros/meridian/commit/1ed9011e7d8aea31956a60c0ef6d7edaaaef3980))
* and there was boot ([7c9a2da](https://github.com/seuros/meridian/commit/7c9a2da08bb956038832edc5d9e6eda785501a9b))
* **app:** three standalone EFI applications ([c116ffa](https://github.com/seuros/meridian/commit/c116ffaf6c924a46d4f7e2e34d6c841e0becedfd))
* **ci:** build, package and publish releases ([d9b469f](https://github.com/seuros/meridian/commit/d9b469f189a0770b61ab43902ad5c5754f91ba49))
* **conn:** three themes, and a menu that spells FreeBSD correctly ([d6349c5](https://github.com/seuros/meridian/commit/d6349c5696003102c6120875ae757d06b9ac3d70))
* **discovery:** recognise Omarchy instead of calling it Arch ([ad09178](https://github.com/seuros/meridian/commit/ad091782adcefa608fed523c4267d906015a3d82))
* **drivers:** load UEFI drivers and connect what they bind ([6b87319](https://github.com/seuros/meridian/commit/6b8731976626cdf2465fae71c67a15eb6f091378))
* **filesystems:** a driver framework, plus UFS and bcachefs ([cd171a4](https://github.com/seuros/meridian/commit/cd171a48c093f8c4c2a0bdf73aca433f47851f52))
* **fsm:** build the state machine runtime as an EDK2 library ([9711a77](https://github.com/seuros/meridian/commit/9711a77a3625d4966725d2b0b43fbefadb689f42))
* **install:** install Meridian onto an ESP from Meridian ([eed2c66](https://github.com/seuros/meridian/commit/eed2c6611ef6ba8da2b934c805e6d9f4035df831))
* **lua:** embed Lua 5.4 in the firmware image ([a1bb85e](https://github.com/seuros/meridian/commit/a1bb85e320ef1bf12633fd8ae4b6db8b8acd83f0))
* **net:** SNP discovery, and the makefiles it does not need ([c5ba48e](https://github.com/seuros/meridian/commit/c5ba48ec386d17b31b5347a84954d37a5b48f38f))
* **policy:** keep NVRAM writes in one place ([dfacc7b](https://github.com/seuros/meridian/commit/dfacc7bfca38035450f04930186ae57bd3647357))
* **remote:** netboot, for the machines that will allow it ([db5146e](https://github.com/seuros/meridian/commit/db5146e83c8e1ae95cb5807e0882b93796fd28ec))
* **tools:** the second row of the menu ([50ea0a9](https://github.com/seuros/meridian/commit/50ea0a95226623bd9e889e9da1f3bc61c852c364))


### Fixes

* **build:** make the AArch64 target compile again ([f3ce64b](https://github.com/seuros/meridian/commit/f3ce64bb892503e48087477692437d5b077117bd))
* **conn:** let people actually see it, and fix what they would have seen ([350ec6d](https://github.com/seuros/meridian/commit/350ec6d5034154973d4fe029c5081eb8e61bc623))


### Refactoring

* **apfs:** MeridianApfsLib replaces the inherited RP_ApfsLib ([a740a5d](https://github.com/seuros/meridian/commit/a740a5d00d932d0260e87f8d6e860f01e2b7b6bb))
* **efilib:** drop the BIOS era, license what stays ([00eba76](https://github.com/seuros/meridian/commit/00eba76343b0b376fcafd2f37682be82b0bb2c58))
* **include:** delete the embedded banners and the call wrappers ([ab6a6ea](https://github.com/seuros/meridian/commit/ab6a6ea3a9d2b374ab76bec9c30e6d8fd5d5a379))
* retire BootMaster, the John Titor wing of the codebase ([07ec0e8](https://github.com/seuros/meridian/commit/07ec0e868ea4c6687befb8424132f22460776907))
* retire libeg, dynasty over, bring in something from the future ([c6583a6](https://github.com/seuros/meridian/commit/c6583a6a7317369af2f44d00e29e1153eea7a06e))


### Documentation

* burn the old propaganda, ratify the new constitution ([d45fac0](https://github.com/seuros/meridian/commit/d45fac02ef8f4eca938d343f6e3742a5ff43cb05))


### Build

* bring in the cavalry, civilise the development surface ([48918d1](https://github.com/seuros/meridian/commit/48918d1b41c499bdbd4742cc2c36aa12733c3f6e))


### Continuous Integration

* move off the Node 20 runtime GitHub is retiring ([c6f414c](https://github.com/seuros/meridian/commit/c6f414cfcfa594cbdf878805840049122ff75824))
* reject the bad traditions, keep the automation that earns its place ([d22a36e](https://github.com/seuros/meridian/commit/d22a36e59f5443c5da2e4be478687dbd3c195214))
* run the Conn tests on every push, through mise ([3459f55](https://github.com/seuros/meridian/commit/3459f555e7e62a343b06d395064b9a8a8e17b074))


### Chores

* add SPDX licence texts and REUSE manifest ([58ee59a](https://github.com/seuros/meridian/commit/58ee59a1181dd995fbb935a3eed5d0de7d7457f8))
* evict 25 crypto wallets holding zero funds ([38e22b7](https://github.com/seuros/meridian/commit/38e22b71aee33f6bd0f6545594d7a5a23fc1902d))
* **library:** license and tidy the inherited MemLog and NvmExpress libs ([35987ae](https://github.com/seuros/meridian/commit/35987ae2d5db769c71ad0a6432dbfc4462b4a427))
* **mok:** license the Machine Owner Key support ([4ea8c06](https://github.com/seuros/meridian/commit/4ea8c06f2f44688e81ff89098f576b6584efecf6))
* reject the inherited art, superior art is on the way ([356957f](https://github.com/seuros/meridian/commit/356957f7e53435e0ae7366b9bef968af3a8b7e5b))
* remove the GPT model nobody asked for ([6eb50c5](https://github.com/seuros/meridian/commit/6eb50c5ea176f58c1bf0d45dcf976ef84ab5fd9f))
* rename the package from RefindPlus to Meridian ([5475dbd](https://github.com/seuros/meridian/commit/5475dbdcf3e2cdc6bb0dc7158fe8ab70a21ca31f))
